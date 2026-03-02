# Potyplex EEG - Web Application

Real-time EEG visualization and recording using FastAPI + React.

## Architecture

```
┌─────────────┐     UDP      ┌─────────────┐   WebSocket   ┌─────────────┐
│   ESP32     │─────────────►│   Backend   │──────────────►│  Frontend   │
│  (WiFi AP)  │   12345      │  (FastAPI)  │    8000       │   (React)   │
└─────────────┘              └─────────────┘               └─────────────┘
                                   │
                              ┌────┴────┐
                              │ Camera  │
                              │ (USB)   │
                              └─────────┘
```

## Features

- Real-time 8-channel EEG visualization
- USB camera streaming
- Recording (signals, video, or both)
- Offline recording playback
- Dark/Light theme

## Requirements

- Python 3.8+
- Node.js 18+
- OpenCV (for camera support)
- Modern browser (Chrome, Firefox, Edge)

## Quick Start

```bash
./run.sh
```

Or manually:

```bash
# Terminal 1 - Backend
cd backend
uvicorn server:app --reload --host 0.0.0.0 --port 8000

# Terminal 2 - Frontend
cd frontend
npm install
npm run dev
```

## Usage

1. Connect to ESP32 WiFi: `Potyplex-EEG` (password: `eeg12345`)
2. Open http://localhost:3000
3. Click **Start** to begin EEG streaming
4. Click **Start Camera** to enable video
5. Select checkboxes and click **REC** to record

## API Endpoints

| Method | Endpoint | Description |
|--------|----------|-------------|
| POST | `/start` | Start EEG streaming |
| POST | `/stop` | Stop EEG streaming |
| POST | `/record/start` | Start recording |
| POST | `/record/stop` | Stop recording |
| GET | `/recordings` | List recordings |
| GET | `/recordings/{id}` | Get recording data |
| GET | `/recordings/{id}/video` | Stream video (MP4) |
| DELETE | `/recordings/{id}` | Delete recording |
| GET | `/cameras` | List cameras |
| POST | `/camera/start` | Start camera |
| POST | `/camera/stop` | Stop camera |
| WS | `/ws` | EEG data stream |
| WS | `/ws/camera` | Camera stream |

## Project Structure

```
software-web/
├── backend/
│   ├── server.py          # FastAPI server
│   ├── requirements.txt   # Python dependencies
│   └── recordings/        # Saved recordings
├── frontend/
│   ├── src/
│   │   ├── App.jsx        # Main component
│   │   ├── App.css        # Styles
│   │   └── components/
│   │       ├── EEGChart.jsx       # EEG visualization
│   │       ├── CameraPanel.jsx    # Camera controls
│   │       └── RecordingsTab.jsx  # Recording viewer
│   ├── package.json
│   └── vite.config.js
├── run.sh                 # Quick start script
└── README.md
```

## Recording Format

Recordings are stored in `backend/recordings/YYYYMMDD_HHMMSS/`:

- `metadata.json` - Session info (duration, channels, etc.)
- `data.csv` - EEG signals (if recorded)
- `video.mp4` - Camera video (if recorded)

## Data Protocol

### UDP (ESP32 → Backend)

OpenBCI packet (33 bytes):
- Byte 0: Header (0xA0)
- Byte 1: Sample number
- Bytes 2-25: 8 channels (3 bytes each, 24-bit signed)
- Bytes 26-31: Accelerometer
- Byte 32: Footer (0xC0)

### WebSocket (Backend → Frontend)

```json
{
  "type": "batch",
  "samples": [
    {"s": 1, "c": [1.2, -3.4, ...], "a": [100, -50, 980]}
  ]
}
```

## Troubleshooting

**Backend not receiving data:**
- Verify connection to ESP32 WiFi
- Check ESP32 LED is blinking

**Camera not working:**
- Check USB camera is connected
- Verify OpenCV is installed: `pip install opencv-python`

**Video not playing:**
- Recordings use MP4 format (mp4v codec)
- Supported by all modern browsers
