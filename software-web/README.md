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
- Cross-platform (Linux, Windows, macOS)

## Requirements

- Python 3.8+
- Node.js 18+
- OpenCV (for camera support)
- Modern browser (Chrome, Firefox, Edge)

## Quick Start

```bash
python run.py
```

This opens an interactive menu to start/stop services, run tests, and more.

## Manual Start

```bash
# Terminal 1 - Backend
cd backend
pip install -r requirements.txt
uvicorn server:app --reload --host 0.0.0.0 --port 8000

# Terminal 2 - Frontend
cd frontend
npm install
npm run dev
```

## Usage

1. Connect to ESP32 WiFi: `Potyplex-EEG` (password: `eeg12345`)
2. Run `python run.py` and select "Start All"
3. Open http://localhost:3000
4. Click **Start** to begin EEG streaming
5. Click **Start Camera** to enable video (optional)
6. Select **Signals** and/or **Video** checkboxes
7. Click **REC** to record

## Menu Options

| Key | Action |
|-----|--------|
| 1 | Start All (backend + frontend) |
| 2 | Stop All |
| 3 | Restart All |
| 4 | Start Backend only |
| 5 | Stop Backend |
| 6 | Stop Frontend |
| 7 | View Backend Logs |
| 8 | View Frontend Logs |
| 9 | Install Dependencies |
| t | Run Tests |
| s | Setup Python Environment |
| k | Kill Stale Processes |
| 0 | Exit |

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
├── run.py             # Cross-platform service manager
├── backend/
│   ├── server.py      # FastAPI server
│   ├── test_server.py # Backend tests (20 tests)
│   ├── requirements.txt
│   └── recordings/    # Saved recordings
├── frontend/
│   ├── src/
│   │   ├── App.jsx
│   │   ├── App.css
│   │   └── components/
│   │       ├── EEGChart.jsx
│   │       ├── CameraPanel.jsx
│   │       └── RecordingsTab.jsx
│   ├── package.json
│   └── vite.config.js
└── README.md
```

## Recording Format

Recordings are stored in `backend/recordings/YYYYMMDD_HHMMSS/`:

- `metadata.json` - Session info (duration, channels, etc.)
- `data.csv` - EEG signals (if recorded)
- `video.mp4` - Camera video (if recorded)

## Testing

Run backend tests:
```bash
python run.py  # then press 't'
```

Or directly:
```bash
cd backend
pytest test_server.py -v
```

## Troubleshooting

**Backend not receiving data:**
- Verify connection to ESP32 WiFi
- Check ESP32 LED is blinking

**Camera not working:**
- Check USB camera is connected
- Verify OpenCV is installed: `pip install opencv-python`

**Services won't start:**
- Press 'k' to kill stale processes
- Check logs with options 7/8
