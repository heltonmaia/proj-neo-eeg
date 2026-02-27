# Neo-EEG Project

Open-source EEG acquisition system based on ADS1299 and ESP32.

## Hardware

| Component | Description |
|-----------|-------------|
| MCU | ESP32 |
| ADC | ADS1299 (8 channels, 24-bit) |
| Connection | Bluetooth SPP |
| PCB | PotyEEG v9 |

## Project Structure

```
proj-neo-eeg/
├── docs/
│   ├── datasheets/             # ADS1299 datasheets
│   └── troubleshooting.md      # Troubleshooting guide
│
├── firmware/
│   ├── esp32/                  # ESP32 firmware (Arduino)
│   │   ├── EEG_Poty_ESP32_V10.ino
│   │   ├── EEG_Poty_ESP32_Library.cpp
│   │   ├── EEG_Poty_ESP32_Library.h
│   │   └── EEG_Poty_ESP32_Library_Definitions.h
│   └── libraries/              # Arduino dependencies
│
├── hardware/
│   └── pcb/
│       └── PotyEEG_v9/         # Eagle PCB files
│           ├── PotyEEG_v9.sch  # Schematic
│           ├── PotyEEG_v9.brd  # Board layout
│           ├── PotyEEG_v9_BOM.xlsx
│           └── *_Gerber.zip    # Manufacturing files
│
├── scripts/
│   └── connect-bluetooth.sh    # Bluetooth connection script
│
└── software/
    └── openbci-gui/            # Modified OpenBCI GUI
        └── OpenBCI_GUI/        # Processing sketch
```

## Quick Start

### 1. Flash Firmware (first time only)

1. Open Arduino IDE
2. Install ESP32 board support
3. Copy `firmware/libraries/*` to your Arduino libraries folder
4. Open `firmware/esp32/EEG_Poty_ESP32_V10.ino`
5. Select board: ESP32 Dev Module
6. Upload

### 2. Connect via Bluetooth

```bash
./scripts/connect-bluetooth.sh
```

### 3. Run OpenBCI GUI

```bash
./scripts/open-gui.sh
```

Configure:
1. **DATA SOURCE**: `CYTON (live)`
2. **TRANSFER PROTOCOL**: `Serial (from Dongle)`
3. **SERIAL CONNECT**: `Manual >` → `REFRESH LIST` → Select `/dev/rfcomm0`
4. Click **START SESSION**

## Firmware

The ESP32 firmware implements the OpenBCI Cyton protocol over Bluetooth Serial:

- **Protocol**: OpenBCI binary format
- **Sample Rate**: 250 Hz
- **Channels**: 8 (ADS1299)
- **Bluetooth**: SPP (Serial Port Profile)

## Hardware Modifications (OpenBCI GUI)

Modified `software/openbci-gui/OpenBCI_GUI/ControlPanel.pde` to support additional serial devices:

```java
final String[] names = {"FT231X USB UART", "VCP", "USB Serial", "CH340", "ttyUSB", "rfcomm"};
```

## Troubleshooting

See [docs/troubleshooting.md](docs/troubleshooting.md)

## Dependencies

### Firmware
- Arduino IDE 2.x
- ESP32 Board Support
- Libraries in `firmware/libraries/`

### Software
- Processing IDE 4.x
- Linux: `bluez`, `rfcomm`
- User in `dialout` group

## License

- OpenBCI GUI: MIT License
- Firmware: See individual library licenses
