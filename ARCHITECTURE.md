# SkyPatch OTA - Architecture Documentation

## 🏗️ System Architecture

```
┌─────────────────────────────────────────────────────────────────────┐
│                        Internet / WiFi                              │
└─────────────────────────────────────────────────────────────────────┘
                                ↑↓
        ┌───────────────────────────────────────────────┐
        │                                               │
        │         Backend Server (FastAPI)              │
        │         http://192.168.1.100:8000             │
        │                                               │
        │  ┌─────────────────────────────────────────┐  │
        │  │  API Endpoints                          │  │
        │  │  • /api/check-update                    │  │
        │  │  • /api/download-firmware               │  │
        │  │  • /api/upload-firmware                 │  │
        │  │  • /api/register-device                 │  │
        │  │  • /api/device-status                   │  │
        │  └─────────────────────────────────────────┘  │
        │                     ↓                          │
        │  ┌─────────────────────────────────────────┐  │
        │  │  Data Storage                           │  │
        │  │  • firmware/                            │  │
        │  │    ├── firmware_v1.0.0.bin              │  │
        │  │    ├── firmware_v1.0.1.bin              │  │
        │  │  • versions.json                        │  │
        │  │  • devices.json                         │  │
        │  └─────────────────────────────────────────┘  │
        │                                               │
        └───────────────────────────────────────────────┘
                    ↑↓ (HTTP GET/POST)
    ┌───────────────────────────────────────────────────────────┐
    │                                                           │
    │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐   │
    │  │  ESP8266_001 │  │  ESP8266_002 │  │  ESP8266_003 │   │
    │  │  (LivingRm)  │  │  (Bedroom)   │  │  (Kitchen)   │   │
    │  └──────────────┘  └──────────────┘  └──────────────┘   │
    │                                                           │
    │              Embedded Devices (IoT)                      │
    └───────────────────────────────────────────────────────────┘
```

## 🔄 Update Workflow

1.  **Boot:** ESP8266 connects to WiFi.
2.  **Register:** Device registers itself to the backend via `/api/register-device`.
3.  **Check:** Device periodically calls `/api/check-update` with its `current_version`.
4.  **Decision:** 
    - If `update_available` is `false`: Wait for next cycle.
    - If `update_available` is `true`: Proceed to download.
5.  **Download:** Device downloads binary from `/api/download-firmware`.
6.  **Verify:** Device calculates MD5 of downloaded file and compares with backend value.
7.  **Flash:** If MD5 matches, device flashes new firmware and reboots.
8.  **Report:** After reboot, device reports its new version to the backend.

## 💾 Data Structures

### versions.json
Stores all available firmware versions, their file paths, MD5 hashes, and metadata.

### devices.json
Tracks registered devices, their last seen version, and update status (success/failed/pending).

## 🔐 Security Layers

- **Integrity:** MD5 checksum ensures the binary isn't corrupted during transit.
- **Rollback:** If an update fails, the ESP8266 logic (via `ESPhttpUpdate`) attempts to stay on the working version.
- **Mandatory Flag:** Backend can flag updates as mandatory, which the client-side logic can prioritize.

---

**Last Updated:** June 2024
