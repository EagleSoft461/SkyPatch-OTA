"""
SkyPatch OTA - Backend (v2.2)
Düzeltme: Yetkilendirme sistemi Dashboard ile tam uyumlu hale getirildi.
"""

from fastapi import FastAPI, File, UploadFile, HTTPException, Query, Header, Depends, Request
from fastapi.responses import FileResponse, JSONResponse
from fastapi.middleware.cors import CORSMiddleware
import os, json, hashlib, secrets, logging
from datetime import datetime
from pathlib import Path
import sqlite3

# Konfigürasyon
FIRMWARE_DIR = Path("firmware")
FIRMWARE_DIR.mkdir(exist_ok=True)
DB_FILE = Path("skypatch_ota.db")
ADMIN_API_KEY = "skypatch_admin_key_2024_secure"

logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger("SkyPatch OTA")

def init_database():
    conn = sqlite3.connect(DB_FILE)
    cursor = conn.cursor()
    cursor.execute('CREATE TABLE IF NOT EXISTS devices (device_id TEXT PRIMARY KEY, name TEXT, registered_at TIMESTAMP, last_seen TIMESTAMP, current_version TEXT, update_status TEXT)')
    cursor.execute('CREATE TABLE IF NOT EXISTS api_keys (key TEXT PRIMARY KEY, device_id TEXT, created_at TIMESTAMP, is_active BOOLEAN)')
    cursor.execute('CREATE TABLE IF NOT EXISTS update_logs (id INTEGER PRIMARY KEY AUTOINCREMENT, device_id TEXT, timestamp TIMESTAMP, from_version TEXT, to_version TEXT, status TEXT, error_message TEXT)')
    cursor.execute('CREATE TABLE IF NOT EXISTS versions (version TEXT PRIMARY KEY, filename TEXT, md5 TEXT, release_date TIMESTAMP, description TEXT, file_size INTEGER)')
    conn.commit()
    conn.close()

init_database()

app = FastAPI(title="SkyPatch OTA v2.2")
app.add_middleware(CORSMiddleware, allow_origins=["*"], allow_methods=["*"], allow_headers=["*"])

@app.get("/")
async def root():
    return {"status": "Çalışıyor ✓", "project": "SkyPatch OTA"}

@app.post("/api/register-device")
async def register_device(device_id: str = Query(...), device_name: str = Query("Unknown")):
    conn = sqlite3.connect(DB_FILE)
    cursor = conn.cursor()
    cursor.execute('SELECT device_id FROM devices WHERE device_id = ?', (device_id,))
    if cursor.fetchone():
        cursor.execute('SELECT key FROM api_keys WHERE device_id = ? AND is_active = 1', (device_id,))
        key_row = cursor.fetchone()
        api_key = key_row[0] if key_row else f"sk_{secrets.token_urlsafe(24)}"
        if not key_row:
            cursor.execute('INSERT INTO api_keys VALUES (?, ?, ?, 1)', (api_key, device_id, datetime.now().isoformat()))
            conn.commit()
        conn.close()
        return {"status": "exists", "api_key": api_key}
    
    cursor.execute('INSERT INTO devices VALUES (?, ?, ?, ?, ?, ?)', (device_id, device_name, datetime.now().isoformat(), datetime.now().isoformat(), "1.0.0", "pending"))
    api_key = f"sk_{secrets.token_urlsafe(24)}"
    cursor.execute('INSERT INTO api_keys VALUES (?, ?, ?, 1)', (api_key, device_id, datetime.now().isoformat()))
    conn.commit()
    conn.close()
    return {"status": "success", "api_key": api_key}

@app.get("/api/check-update")
async def check_update(device_id: str = Query(...), current_version: str = Query(...), device_key: str = Header(None)):
    conn = sqlite3.connect(DB_FILE)
    cursor = conn.cursor()
    cursor.execute('SELECT device_id FROM api_keys WHERE key = ? AND is_active = 1', (device_key,))
    if not cursor.fetchone():
        conn.close()
        raise HTTPException(status_code=401)
    
    cursor.execute('UPDATE devices SET last_seen = ?, current_version = ? WHERE device_id = ?', (datetime.now().isoformat(), current_version, device_id))
    cursor.execute('SELECT version FROM versions ORDER BY release_date DESC LIMIT 1')
    row = cursor.fetchone()
    conn.commit()
    conn.close()
    latest = row[0] if row else "1.0.0"
    return {"update_available": current_version < latest, "latest_version": latest}

@app.get("/api/download-firmware")
async def download_firmware(version: str = Query(...), device_id: str = Query(...), device_key: str = Header(None)):
    conn = sqlite3.connect(DB_FILE)
    cursor = conn.cursor()
    cursor.execute('SELECT device_id FROM api_keys WHERE key = ? AND device_id = ?', (device_key, device_id))
    if not cursor.fetchone():
        conn.close()
        raise HTTPException(status_code=401)
    cursor.execute('SELECT filename FROM versions WHERE version = ?', (version,))
    row = cursor.fetchone()
    conn.close()
    if not row: raise HTTPException(status_code=404)
    return FileResponse(FIRMWARE_DIR / row[0])

@app.get("/api/devices")
async def list_devices(admin_key: str = Query(None)):
    if admin_key != ADMIN_API_KEY: raise HTTPException(status_code=403)
    conn = sqlite3.connect(DB_FILE)
    cursor = conn.cursor()
    cursor.execute('SELECT * FROM devices')
    cols = [c[0] for c in cursor.description]
    data = [dict(zip(cols, r)) for r in cursor.fetchall()]
    conn.close()
    return {"total": len(data), "devices": data}

@app.post("/api/upload-firmware")
async def upload_firmware(version: str = Query(...), description: str = Query(""), file: UploadFile = File(...), admin_key: str = Query(None)):
    if admin_key != ADMIN_API_KEY: raise HTTPException(status_code=403)
    filename = f"firmware_v{version}.bin"
    content = await file.read()
    with open(FIRMWARE_DIR / filename, "wb") as f: f.write(content)
    conn = sqlite3.connect(DB_FILE)
    cursor = conn.cursor()
    cursor.execute('INSERT OR REPLACE INTO versions VALUES (?,?,?,?,?,?)', (version, filename, hashlib.md5(content).hexdigest(), datetime.now().isoformat(), description, len(content)))
    conn.commit()
    conn.close()
    return {"status": "success"}

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
