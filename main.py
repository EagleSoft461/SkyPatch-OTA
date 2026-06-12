"""
SkyPatch OTA - Backend (v2.4 - Public Ready)
"""
from fastapi import FastAPI, File, UploadFile, HTTPException, Query, Header
from fastapi.responses import FileResponse
from fastapi.middleware.cors import CORSMiddleware
import os, hashlib, secrets, logging, sqlite3
from datetime import datetime
from pathlib import Path

FIRMWARE_DIR = Path("firmware")
FIRMWARE_DIR.mkdir(exist_ok=True)
DB_FILE = Path("skypatch_ota.db")

# SECURITY: Get Admin Key from Environment Variable or use a placeholder
ADMIN_API_KEY = os.getenv("SKYPATCH_ADMIN_KEY", "change_me_in_production")

logging.basicConfig(level=logging.INFO)
logger = logging.getLogger("SkyPatch OTA")

def init_db():
    conn = sqlite3.connect(DB_FILE)
    c = conn.cursor()
    c.execute('CREATE TABLE IF NOT EXISTS devices (device_id TEXT PRIMARY KEY, name TEXT, registered_at TIMESTAMP, last_seen TIMESTAMP, current_version TEXT, update_status TEXT)')
    c.execute('CREATE TABLE IF NOT EXISTS api_keys (key TEXT PRIMARY KEY, device_id TEXT, created_at TIMESTAMP, is_active BOOLEAN)')
    c.execute('CREATE TABLE IF NOT EXISTS versions (version TEXT PRIMARY KEY, filename TEXT, md5 TEXT, release_date TIMESTAMP, description TEXT, file_size INTEGER)')
    conn.commit()
    conn.close()

init_db()
app = FastAPI()
app.add_middleware(CORSMiddleware, allow_origins=["*"], allow_methods=["*"], allow_headers=["*"])

@app.get("/")
async def root(): return {"status": "SkyPatch OTA is Running"}

@app.get("/api/register-device")
async def register(device_id: str = Query(...), device_name: str = Query("Unknown")):
    conn = sqlite3.connect(DB_FILE)
    c = conn.cursor()
    c.execute('SELECT key FROM api_keys WHERE device_id = ?', (device_id,))
    row = c.fetchone()
    if row: 
        conn.close()
        return {"api_key": row[0]}
    
    api_key = f"sk_{secrets.token_urlsafe(24)}"
    c.execute('INSERT INTO devices VALUES (?,?,?,?,?,?)', (device_id, device_name, datetime.now().isoformat(), datetime.now().isoformat(), "1.0.0", "pending"))
    c.execute('INSERT INTO api_keys VALUES (?,?,?,1)', (api_key, device_id, datetime.now().isoformat()))
    conn.commit()
    conn.close()
    return {"api_key": api_key}

@app.get("/api/devices")
async def list_devices(admin_key: str = Query(None)):
    if admin_key != ADMIN_API_KEY: raise HTTPException(status_code=403)
    conn = sqlite3.connect(DB_FILE)
    c = conn.cursor()
    c.execute('SELECT * FROM devices')
    cols = [d[0] for d in c.description]
    data = [dict(zip(cols, r)) for r in c.fetchall()]
    conn.close()
    return {"devices": data}

@app.get("/api/check-update")
async def check(device_id: str = Query(...), current_version: str = Query(...), device_key: str = Header(None)):
    conn = sqlite3.connect(DB_FILE)
    c = conn.cursor()
    c.execute('SELECT device_id FROM api_keys WHERE key = ?', (device_key,))
    if not c.fetchone(): raise HTTPException(status_code=401)
    c.execute('UPDATE devices SET last_seen = ?, current_version = ? WHERE device_id = ?', (datetime.now().isoformat(), current_version, device_id))
    c.execute('SELECT version FROM versions ORDER BY release_date DESC LIMIT 1')
    row = c.fetchone()
    conn.commit(); conn.close()
    latest = row[0] if row else "1.0.0"
    return {"update_available": current_version < latest, "latest_version": latest}

@app.get("/api/download-firmware")
async def download(version: str = Query(...), device_id: str = Query(...), device_key: str = Header(None)):
    conn = sqlite3.connect(DB_FILE)
    c = conn.cursor()
    c.execute('SELECT device_id FROM api_keys WHERE key = ? AND device_id = ?', (device_key, device_id))
    if not c.fetchone(): raise HTTPException(status_code=401)
    c.execute('SELECT filename FROM versions WHERE version = ?', (version,))
    row = c.fetchone()
    conn.close()
    if not row: raise HTTPException(status_code=404)
    return FileResponse(FIRMWARE_DIR / row[0])

@app.post("/api/upload-firmware")
async def upload(version: str = Query(...), admin_key: str = Query(None), file: UploadFile = File(...)):
    if admin_key != ADMIN_API_KEY: raise HTTPException(status_code=403)
    content = await file.read()
    filename = f"v{version}.bin"
    with open(FIRMWARE_DIR / filename, "wb") as f: f.write(content)
    conn = sqlite3.connect(DB_FILE)
    c = conn.cursor()
    c.execute('INSERT OR REPLACE INTO versions VALUES (?,?,?,?,?,?)', (version, filename, hashlib.md5(content).hexdigest(), datetime.now().isoformat(), "", len(content)))
    conn.commit(); conn.close()
    return {"status": "success"}

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
