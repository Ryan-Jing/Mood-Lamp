import hashlib
import os
import secrets
import sqlite3
import time

DB_PATH = os.getenv("MOOD_DB", "mood.db")
PAIR_ID = os.getenv("PAIR_ID", "pair_001")
DEVICES = [("lamp_a", "Mood Lamp A"), ("lamp_b", "Mood Lamp B")]

def token_hash(token: str) -> str:
    return hashlib.sha256(token.encode("utf-8")).hexdigest()

con = sqlite3.connect(DB_PATH)
con.execute("PRAGMA foreign_keys = ON")

con.executescript("""
CREATE TABLE IF NOT EXISTS pairs (
    pair_id TEXT PRIMARY KEY,
    created_at INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS devices (
    device_id TEXT PRIMARY KEY,
    pair_id TEXT NOT NULL,
    display_name TEXT NOT NULL,
    token_hash TEXT UNIQUE NOT NULL,
    created_at INTEGER NOT NULL,
    last_seen INTEGER,
    FOREIGN KEY(pair_id) REFERENCES pairs(pair_id)
);

CREATE TABLE IF NOT EXISTS mood_state (
    device_id TEXT PRIMARY KEY,
    mood_id INTEGER NOT NULL,
    version INTEGER NOT NULL,
    updated_at INTEGER NOT NULL,
    FOREIGN KEY(device_id) REFERENCES devices(device_id) ON DELETE CASCADE
);
""")

now = int(time.time())
con.execute(
    "INSERT OR IGNORE INTO pairs(pair_id, created_at) VALUES (?, ?)",
    (PAIR_ID, now),
)

print("Raw device tokens. Save these now; they are not recoverable later.\n")

for device_id, display_name in DEVICES:
    exists = con.execute(
        "SELECT 1 FROM devices WHERE device_id = ?", (device_id,)
    ).fetchone()

    if exists:
        print(f"{device_id}: already exists, token not changed")
        continue

    token = secrets.token_urlsafe(32)
    con.execute("""
        INSERT INTO devices(device_id, pair_id, display_name, token_hash, created_at)
        VALUES (?, ?, ?, ?, ?)
    """, (device_id, PAIR_ID, display_name, token_hash(token), now))

    con.execute("""
        INSERT INTO mood_state(device_id, mood_id, version, updated_at)
        VALUES (?, 0, 1, ?)
    """, (device_id, now))

    print(f"{device_id}: {token}")

con.commit()
con.close()