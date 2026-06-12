# SkyPatch OTA - ESP8266 Over-the-Air Update System

A comprehensive **FastAPI** and **Arduino** based OTA update system designed to manage and update thousands of ESP8266 devices remotely without physical access.

## 🎯 Project Goals

This project allows you to manage IoT device firmware via a **centralized backend server**:

- **Remote Updates:** Devices periodically check the backend to download new firmware.
- **Version Management:** Track all firmware versions and device statuses in one place.
- **Security:** Secure updates using MD5 hash verification and versioning.
- **Scalability:** Manage thousands of devices simultaneously.

## 📋 System Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                   Backend (FastAPI)                         │
│  ┌────────────────────────────────────────────────────────┐ │
│  │ • Firmware Version Management                          │ │
│  │ • Device Registration and Tracking                     │ │
│  │ • Update Check API                                     │ │
│  │ • Firmware Download Service                            │ │
│  └────────────────────────────────────────────────────────┘ │
│                          ↑↓ (HTTP)                           │
│  ┌────────────────────────────────────────────────────────┐ │
│  │ • Firmware Files (firmware/)                           │ │
│  │ • Version Info (versions.json)                         │ │
│  │ • Device Info (devices.json)                          │ │
│  └────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
                            ↑↓
┌─────────────────────────────────────────────────────────────┐
│              Embedded Devices (ESP8266)                     │
│  ┌────────────────────────────────────────────────────────┐ │
│  │ • WiFi Connectivity                                    │ │
│  │ • Update Check (Periodic)                              │ │
│  │ • Firmware Download and Verification                   │ │
│  │ • Secure Reboot                                        │ │
│  └────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────┘
```

## 🚀 Quick Start

### Prerequisites

- **Backend:** Python 3.8+, FastAPI, Uvicorn
- **Embedded:** ESP8266, Arduino IDE, WiFi connection
- **General:** Git, curl, or Postman (for testing)

### 1. Backend Setup

```bash
# Navigate to project directory
cd esp8266-ota-system

# Install Python dependencies
pip install -r requirements.txt

# Start the backend server
python main.py
```

The server will run at `http://localhost:8000`.

**To access API Documentation:**
```
http://localhost:8000/docs
```

### 2. ESP8266 Setup

#### Setup in Arduino IDE

1. **Open Arduino IDE** → `File` → `Preferences`
2. **Add Board Manager URL:**
   ```
   http://arduino.esp8266.com/stable/package_esp8266com_index.json
   ```
3. **Install ESP8266 package from Boards Manager:**
   - `Tools` → `Board` → `Boards Manager`
   - Search for "esp8266" and install.

#### Upload Code

1. Open `esp8266_ota_client.ino` in Arduino IDE.
2. **Configure settings:**
   ```cpp
   const char* SSID = "YOUR_SSID";           // WiFi name
   const char* PASSWORD = "YOUR_PASSWORD";   // WiFi password
   const char* SERVER_IP = "192.168.1.100";  // Backend IP
   const char* DEVICE_ID = "ESP8266_001";    // Device ID
   ```
3. **Select Board:** `Tools` → `Board` → `NodeMCU 1.0 (ESP-12E Module)`
4. **Select Port:** `Tools` → `Port` → Your COM/ttyUSB port.
5. **Upload:** `Sketch` → `Upload` (or Ctrl+U).

## 📚 API Reference

### 1. Device Registration

**POST** `/api/register-device`

Register a new device to the system.

```bash
curl -X POST "http://localhost:8000/api/register-device?device_id=ESP8266_001&device_name=LivingRoom_Sensor"
```

### 2. Update Check

**GET** `/api/check-update`

Check if there is a new update for the device.

```bash
curl "http://localhost:8000/api/check-update?device_id=ESP8266_001&current_version=1.0.0"
```

### 3. Firmware Upload

**POST** `/api/upload-firmware`

Upload a new firmware version.

```bash
curl -X POST \
  -F "version=1.0.1" \
  -F "description=New Features" \
  -F "file=@firmware_v1.0.1.bin" \
  "http://localhost:8000/api/upload-firmware"
```

## 🧪 Testing the System

### Automated Test Suite

```bash
# Ensure backend server is running
python test_ota_system.py
```

## 🔒 Security Features

- **MD5 Hash Verification:** Ensures file integrity.
- **Version Control:** Prevents accidental downgrades.
- **Mandatory Updates:** Force critical security patches.
- **Rollback Mechanism:** Reverts to previous version on failure.

---

**Created:** June 2026  
**Last Updated:** June 2026
