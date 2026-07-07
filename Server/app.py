import hashlib
import os
import sqlite3
import time
from pathlib import Path
from typing import Annotated

from fastapi import Depends, FastAPI, Header, HTTPException, Response
from pydantic import BaseModel, Field

DB_PATH = Path(os.getenv("MOOD_DB", "mood.db"))
MOOD_COUNT = int(os.getenv("MOOD_COUNT", "13"))

app = FastAPI(title="Mood Lamp API", version="0.1.0")

class MoodIn(BaseModel):
    mood_id: int = Field(ge=0, lt=MOOD_COUNT)

def token_hash(token: str) -> str:
    return hashlib.sha256(token.encode("utf-8")).hexdigest()

def get_db():
    con = sqlite3.connect(DB_PATH)
    con.row_factory = sqlite3.Row
    con.execute("PRAGMA foreign_keys = ON")
    try:
        yield con
    finally:
        con.close()

def require_device(
    authorization: Annotated[str | None, Header()] = None,
    con: sqlite3.Connection = Depends(get_db),
):
    if not authorization or not authorization.startswith("Bearer "):
        raise HTTPException(status_code=401, detail="missing bearer token")

    raw_token = authorization.removeprefix("Bearer ").strip()
    row = con.execute("""
        SELECT device_id, pair_id, display_name
        FROM devices
        WHERE token_hash = ?
    """, (token_hash(raw_token),)).fetchone()

    if row is None:
        raise HTTPException(status_code=401, detail="invalid token")

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