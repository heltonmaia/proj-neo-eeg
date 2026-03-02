# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Neo-EEG is an open-source 8-channel EEG acquisition system based on ADS1299 ADC and ESP32 microcontroller. The project includes firmware, hardware designs, and visualization software.

## Architecture

```
ESP32 + ADS1299  ──UDP:12345──►  Python Backend  ──WebSocket:8000──►  React Frontend
   (WiFi AP)                      (FastAPI)                            (Vite)
192.168.4.1                      localhost:8000                      localhost:3000
```

Data flow:
1. ESP32 creates WiFi AP "Potyplex-EEG" and streams 33-byte OpenBCI packets via UDP at 250Hz
2. Backend (`server.py`) receives UDP, parses packets, broadcasts JSON via WebSocket
3. Frontend displays real-time charts using Recharts

## Development Commands

### Web Application (software-web/)

**Quick start with interactive menu:**
```bash
cd software-web
./run.sh
```

**Manual start:**
```bash
# Backend (requires UV environment)
cd software-web/backend
uvicorn server:app --reload --host 0.0.0.0 --port 8000

# Frontend
cd software-web/frontend
npm install
npm run dev
```

**Run tests:**
```bash
cd software-web/backend
pytest test_server.py -v
```

**Build frontend:**
```bash
cd software-web/frontend
npm run build
```

### Firmware (firmware/)

Use Arduino IDE:
1. Board: ESP32 Dev Module
2. Open `firmware/esp32-wifi/EEG_Poty_ESP32_V10/EEG_Poty_ESP32_V10.ino`
3. Upload

WiFi config in `EEG_Poty_ESP32_Library_Definitions.h`:
- SSID: `Potyplex-EEG`
- Password: `eeg12345`
- UDP Port: `12345`

## Key Components

### Backend (software-web/backend/server.py)
- FastAPI with async UDP receiver and WebSocket broadcaster
- EEG packet parser (24-bit signed channels, converts to microvolts)
- Recording to CSV with metadata
- Video recording to MP4 (mp4v codec) with OpenCV
- USB camera capture and streaming

### Frontend (software-web/frontend/src/)
- `App.jsx`: Main component with tabs (Live/Recordings), WebSocket handling, recording controls
- `components/EEGChart.jsx`: Per-channel visualization with zoom controls
- `components/RecordingsTab.jsx`: Offline recording viewer with video playback
- `components/CameraPanel.jsx`: Camera streaming panel with start/stop controls

### Data Protocol
OpenBCI packet (33 bytes):
- Byte 0: Header `0xA0`
- Byte 1: Sample counter
- Bytes 2-25: 8 EEG channels (3 bytes each, 24-bit signed big-endian)
- Bytes 26-31: Accelerometer (3 axes)
- Byte 32: Footer `0xC0`

## API Endpoints

| Method | Endpoint | Description |
|--------|----------|-------------|
| POST | `/start` | Start EEG streaming |
| POST | `/stop` | Stop EEG streaming |
| POST | `/record/start` | Start recording (signals and/or video) |
| POST | `/record/stop` | Stop recording |
| GET | `/recordings` | List recordings |
| GET | `/recordings/{id}` | Get recording data |
| GET | `/recordings/{id}/video` | Stream recording video (MP4) |
| DELETE | `/recordings/{id}` | Delete recording |
| GET | `/cameras` | List available cameras |
| POST | `/camera/start` | Start camera capture |
| POST | `/camera/stop` | Stop camera capture |
| GET | `/stats` | Server statistics |
| WS | `/ws` | EEG data stream |
| WS | `/ws/camera` | Camera frame stream |

## Recording System

Recordings are stored in `software-web/backend/recordings/` with the following structure:
```
recordings/
└── YYYYMMDD_HHMMSS/
    ├── metadata.json    # Session metadata
    ├── data.csv         # EEG signals (if recorded)
    └── video.mp4        # Video (if recorded)
```

Recording options:
- **Signals only**: Records EEG data to CSV
- **Video only**: Records camera to MP4
- **Both**: Records signals and video simultaneously
