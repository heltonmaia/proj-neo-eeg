# Potyplex EEG - Web Viewer

Real-time EEG visualization using FastAPI + React.

## Architecture

```
┌─────────────┐     UDP      ┌─────────────┐   WebSocket   ┌─────────────┐
│   ESP32     │─────────────►│   Backend   │──────────────►│  Frontend   │
│  (WiFi AP)  │   12345      │  (FastAPI)  │    8000       │   (React)   │
└─────────────┘              └─────────────┘               └─────────────┘
```

## Requirements

- Python 3.8+
- Node.js 18+
- Modern browser (Chrome, Firefox, Edge)

## Backend (FastAPI)

The backend receives UDP packets from ESP32, parses OpenBCI format, and broadcasts to WebSocket clients.

### Endpoints

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/` | API info |
| GET | `/stats` | Statistics (samples, packets, clients) |
| POST | `/start` | Send start command to ESP32 |
| POST | `/stop` | Send stop command to ESP32 |
| WS | `/ws` | WebSocket for real-time data |

### Setup

```bash
cd backend
pip install -r requirements.txt
uvicorn server:app --reload --host 0.0.0.0 --port 8000
```

## Frontend (React + Vite)

React application with real-time EEG visualization using Recharts.

### Features

- 8-channel EEG display
- Channel selection
- Start/Stop streaming controls
- Live statistics

### Setup

```bash
cd frontend
npm install
npm run dev
```

Frontend available at: http://localhost:3000

## Usage

### 1. Connect to ESP32

Connect your computer to the ESP32 WiFi network:
- **SSID:** `Potyplex-EEG`
- **Password:** `eeg12345`

### 2. Start Backend

```bash
cd backend
uvicorn server:app --reload --host 0.0.0.0 --port 8000
```

### 3. Start Frontend

```bash
cd frontend
npm run dev
```

### 4. View EEG

1. Open http://localhost:3000
2. Wait for "Connected" status
3. Click **Start** to begin streaming
4. Select channels to display

## Project Structure

```
software-web/
├── backend/
│   ├── server.py          # FastAPI server (UDP + WebSocket)
│   └── requirements.txt   # Python dependencies
├── frontend/
│   ├── src/
│   │   ├── main.jsx           # React entry point
│   │   ├── App.jsx            # Main component
│   │   ├── App.css            # Styles
│   │   └── components/
│   │       └── EEGChart.jsx   # Chart component
│   ├── index.html
│   ├── package.json
│   └── vite.config.js
└── README.md
```

## Data Protocol

### UDP (ESP32 → Backend)

OpenBCI packet format (33 bytes):
- Byte 0: Header (0xA0)
- Byte 1: Sample number
- Bytes 2-25: 8 EEG channels (3 bytes each, 24-bit signed)
- Bytes 26-31: Accelerometer (3 axes)
- Byte 32: Footer (0xC0)

### WebSocket (Backend → Frontend)

JSON format:
```json
{
  "sample": 123,
  "channels": [12.34, -5.67, ...],
  "accel": [100, -50, 980]
}
```

## Troubleshooting

### Backend not receiving data
- Verify connection to ESP32 WiFi network
- Confirm ESP32 is streaming (LED blinking)
- Test with: `nc -u 192.168.4.1 12345` → type `b`

### Frontend not connecting
- Verify backend is running on port 8000
- Check browser console (F12) for errors

### Slow charts
- Reduce number of visible channels
- Close other browser tabs
