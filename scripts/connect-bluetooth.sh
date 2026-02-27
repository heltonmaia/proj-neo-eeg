#!/bin/bash
#
# Connect to Potyplex EEG via Bluetooth
# Creates a virtual serial port at /dev/rfcomm0
#

DEVICE_NAME="Potyplex EEG"
DEVICE_MAC="0C:DC:7E:8E:30:8E"
RFCOMM_PORT=0

echo "=== Potyplex EEG Bluetooth Connection ==="

# Check if already connected
if [ -e "/dev/rfcomm${RFCOMM_PORT}" ]; then
    echo "[INFO] Port /dev/rfcomm${RFCOMM_PORT} already exists"
    echo "[INFO] To reconnect, run: sudo rfcomm release ${RFCOMM_PORT}"
    exit 0
fi

# Check if device is paired
if ! bluetoothctl devices | grep -q "${DEVICE_MAC}"; then
    echo "[ERROR] Device ${DEVICE_NAME} (${DEVICE_MAC}) not paired!"
    echo "[INFO] To pair, run:"
    echo "       bluetoothctl"
    echo "       > scan on"
    echo "       > pair ${DEVICE_MAC}"
    echo "       > trust ${DEVICE_MAC}"
    echo "       > exit"
    exit 1
fi

echo "[INFO] Connecting to ${DEVICE_NAME} (${DEVICE_MAC})..."

# Create rfcomm port
sudo rfcomm bind ${RFCOMM_PORT} ${DEVICE_MAC}

if [ $? -eq 0 ]; then
    echo "[OK] Connected! Serial port: /dev/rfcomm${RFCOMM_PORT}"
    echo ""
    echo "Now open OpenBCI GUI and select /dev/rfcomm${RFCOMM_PORT}"
else
    echo "[ERROR] Failed to create rfcomm port"
    exit 1
fi
