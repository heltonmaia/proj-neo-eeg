# Neo-EEG Project

Open-source 8-channel EEG acquisition system based on ADS1299 and ESP32.

## Hardware

| Component | Description |
|-----------|-------------|
| MCU | ESP32 |
| ADC | ADS1299 (8 channels, 24-bit) |
| Connection | WiFi (UDP) or Bluetooth SPP |
| PCB | PotyEEG v9 |

## Project Structure

```
proj-neo-eeg/
├── docs/
│   ├── datasheets/             # ADS1299 datasheets
│   └── troubleshooting.md      # Troubleshooting guide
│
├── firmware/
│   ├── esp32-wifi/             # ESP32 WiFi firmware
│   │   └── EEG_Poty_ESP32_V10/
│   └── libraries/              # Arduino dependencies
│
├── hardware/
│   └── pcb/
│       └── PotyEEG_v9/         # Eagle PCB files
│
├── software-web/               # Web application (recommended)
│   ├── run.py                  # Cross-platform launcher
│   ├── backend/                # FastAPI server
│   └── frontend/               # React application
│
└── software-processing/        # OpenBCI GUI (legacy)
    └── openbci-gui/
```

## Quick Start (Web Application)

### 1. Flash Firmware

1. Open Arduino IDE
2. Install ESP32 board support
3. Open `firmware/esp32-wifi/EEG_Poty_ESP32_V10/EEG_Poty_ESP32_V10.ino`
4. Select board: ESP32 Dev Module
5. Upload

### 2. Connect to ESP32 WiFi

- **SSID:** `Potyplex-EEG`
- **Password:** `eeg12345`

### 3. Run Web Application

```bash
cd software-web
python run.py
```

Select option `1` to start all services, then open http://localhost:3000

### 4. Use

1. Click **Start** to begin EEG streaming
2. Click **Start Camera** to enable video (optional)
3. Select **Signals** and/or **Video** checkboxes
4. Click **REC** to record

## Features

- Real-time 8-channel EEG visualization
- USB camera streaming
- Recording to CSV (signals) and MP4 (video)
- Offline recording playback
- Dark/Light theme
- Channel selection and zoom controls
- Cross-platform (Linux, Windows, macOS)

## Firmware

The ESP32 firmware streams OpenBCI-compatible packets:

- **Protocol**: OpenBCI binary format (33 bytes)
- **Sample Rate**: 250 Hz
- **Channels**: 8 (ADS1299)
- **Transport**: UDP over WiFi

## Requirements

### Firmware
- Arduino IDE 2.x
- ESP32 Board Support

### Web Application
- Python 3.8+
- Node.js 18+
- OpenCV (for camera)

## License

MIT License
