import hashlib
import os
import sqlite3
import threading
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Annotated

from fastapi import Depends, FastAPI, Header, HTTPException, Request, Response
from fastapi.responses import JSONResponse
from pydantic import BaseModel, Field

DB_PATH = Path(os.getenv("MOOD_DB", "mood.db"))
MOOD_COUNT = int(os.getenv("MOOD_COUNT", "13"))
RATE_LIMIT_WINDOW_SECONDS = int(os.getenv("RATE_LIMIT_WINDOW_SECONDS", "60"))
CLIENT_RATE_LIMIT = int(os.getenv("CLIENT_RATE_LIMIT", "240"))
AUTH_FAILURE_RATE_LIMIT = int(os.getenv("AUTH_FAILURE_RATE_LIMIT", "20"))
DEVICE_RATE_LIMIT = int(os.getenv("DEVICE_RATE_LIMIT", "120"))
MAX_REQUEST_BODY_BYTES = int(os.getenv("MAX_REQUEST_BODY_BYTES", "1024"))

app = FastAPI(
    title="Mood Lamp API",
    version="0.1.0",
    docs_url=None,
    redoc_url=None,
    openapi_url=None,
)


@dataclass
class RateLimitCounter:
    window_id: int
    count: int


rate_limit_lock = threading.Lock()
rate_limit_counters: dict[str, RateLimitCounter] = {}
last_rate_limit_cleanup = 0.0


class MoodIn(BaseModel):
    mood_id: int = Field(ge=0, lt=MOOD_COUNT)


def token_hash(token: str) -> str:
    return hashlib.sha256(token.encode("utf-8")).hexdigest()


def request_client_id(request: Request) -> str:
    if request.client and request.client.host:
        return request.client.host
    return "unknown"


def enforce_rate_limit(
    key: str,
    limit: int,
    window_seconds: int = RATE_LIMIT_WINDOW_SECONDS,
):
    if limit <= 0 or window_seconds <= 0:
        return

    global last_rate_limit_cleanup

    now = time.monotonic()
    window_id = int(now // window_seconds)
    retry_after = window_seconds - int(now % window_seconds)

    with rate_limit_lock:
        counter = rate_limit_counters.get(key)
        if counter is None or counter.window_id != window_id:
            counter = RateLimitCounter(window_id=window_id, count=0)

        counter.count += 1
        rate_limit_counters[key] = counter

        if now - last_rate_limit_cleanup > window_seconds:
            stale_before = window_id - 1
            stale_keys = [
                counter_key
                for counter_key, value in rate_limit_counters.items()
                if value.window_id < stale_before
            ]
            for counter_key in stale_keys:
                del rate_limit_counters[counter_key]
            last_rate_limit_cleanup = now

        if counter.count > limit:
            raise HTTPException(
                status_code=429,
                detail="rate limit exceeded",
                headers={"Retry-After": str(max(1, retry_after))},
            )


@app.middleware("http")
async def enforce_public_limits(request: Request, call_next):
    content_length = request.headers.get("content-length")
    if content_length is not None:
        try:
            body_size = int(content_length)
        except ValueError:
            body_size = MAX_REQUEST_BODY_BYTES + 1

        if body_size > MAX_REQUEST_BODY_BYTES:
            return JSONResponse(
                status_code=413,
                content={"detail": "request body too large"},
            )

    try:
        enforce_rate_limit(
            f"client:{request_client_id(request)}",
            CLIENT_RATE_LIMIT,
        )
    except HTTPException as exc:
        return JSONResponse(
            status_code=exc.status_code,
            content={"detail": exc.detail},
            headers=exc.headers,
        )

    return await call_next(request)


def get_db():
    con = sqlite3.connect(DB_PATH)
    con.row_factory = sqlite3.Row
    con.execute("PRAGMA foreign_keys = ON")
    try:
        yield con
    finally:
        con.close()


def require_device(
    request: Request,
    authorization: Annotated[str | None, Header()] = None,
    con: sqlite3.Connection = Depends(get_db),
):
    if not authorization or not authorization.startswith("Bearer "):
        enforce_rate_limit(
            f"auth-failure:{request_client_id(request)}",
            AUTH_FAILURE_RATE_LIMIT,
        )
        raise HTTPException(status_code=401, detail="missing bearer token")

    raw_token = authorization.removeprefix("Bearer ").strip()
    row = con.execute("""
        SELECT device_id, pair_id, display_name
        FROM devices
        WHERE token_hash = ?
    """, (token_hash(raw_token),)).fetchone()

    if row is None:
        enforce_rate_limit(
            f"auth-failure:{request_client_id(request)}",
            AUTH_FAILURE_RATE_LIMIT,
        )
        raise HTTPException(status_code=401, detail="invalid token")

    enforce_rate_limit(
        f"device:{row['device_id']}",
        DEVICE_RATE_LIMIT,
    )

    con.execute(
        "UPDATE devices SET last_seen = ? WHERE device_id = ?",
        (int(time.time()), row["device_id"]),
    )
    con.commit()
    return row

@app.get("/v1/health")
def health():
    return {"ok": True}

@app.get("/v1/me")
def me(device=Depends(require_device)):
    return {
        "device_id": device["device_id"],
        "pair_id": device["pair_id"],
        "display_name": device["display_name"],
    }

@app.post("/v1/mood")
def post_mood(
    body: MoodIn,
    device=Depends(require_device),
    con: sqlite3.Connection = Depends(get_db),
):
    now = int(time.time())
    con.execute("""
        INSERT INTO mood_state(device_id, mood_id, version, updated_at)
        VALUES (?, ?, 1, ?)
        ON CONFLICT(device_id) DO UPDATE SET
            mood_id = excluded.mood_id,
            version = mood_state.version + 1,
            updated_at = excluded.updated_at
    """, (device["device_id"], body.mood_id, now))

    row = con.execute("""
        SELECT mood_id, version, updated_at
        FROM mood_state
        WHERE device_id = ?
    """, (device["device_id"],)).fetchone()

    con.commit()
    return {
        "ok": True,
        "mood_id": row["mood_id"],
        "version": row["version"],
        "updated_at": row["updated_at"],
    }

@app.get("/v1/peer/mood")
def get_peer_mood(
    response: Response,
    if_none_match: Annotated[str | None, Header()] = None,
    device=Depends(require_device),
    con: sqlite3.Connection = Depends(get_db),
):
    peers = con.execute("""
        SELECT d.device_id, m.mood_id, m.version, m.updated_at
        FROM devices d
        JOIN mood_state m ON m.device_id = d.device_id
        WHERE d.pair_id = ? AND d.device_id != ?
    """, (device["pair_id"], device["device_id"])).fetchall()

    if len(peers) != 1:
        raise HTTPException(status_code=409, detail="pair must have exactly one peer")

    peer = peers[0]
    etag = f'"{peer["version"]}"'

    if if_none_match == etag:
        return Response(status_code=304)

    response.headers["ETag"] = etag
    return {
        "device_id": peer["device_id"],
        "mood_id": peer["mood_id"],
        "version": peer["version"],
        "updated_at": peer["updated_at"],
    }
