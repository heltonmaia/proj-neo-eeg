# Troubleshooting

## Bluetooth Connection Issues

### Device not found
```bash
# Check if device is paired
bluetoothctl devices

# If not listed, pair it:
bluetoothctl
> scan on
> pair 0C:DC:7E:8E:30:8E
> trust 0C:DC:7E:8E:30:8E
> exit
```

### rfcomm port not created
```bash
# Check if port exists
ls -la /dev/rfcomm*

# Release and recreate
sudo rfcomm release 0
sudo rfcomm bind 0 0C:DC:7E:8E:30:8E
```

### Permission denied on serial port
```bash
# Add user to dialout group
sudo usermod -a -G dialout $USER

# Logout and login again
```

## OpenBCI GUI Issues

### Port not appearing in REFRESH LIST

1. Ensure rfcomm port exists: `ls /dev/rfcomm0`
2. Restart the OpenBCI GUI (Stop + Run in Processing)
3. Click REFRESH LIST again

### BOARD_NOT_READY_ERROR

- Ensure Potyplex EEG board is powered on (LED should be lit)
- Check Bluetooth connection is active
- Try disconnecting and reconnecting:
  ```bash
  sudo rfcomm release 0
  sudo rfcomm bind 0 0C:DC:7E:8E:30:8E
  ```

### USB Serial (CH340) not detected

1. Check if device is connected:
   ```bash
   lsusb | grep CH340
   ls /dev/ttyUSB*
   ```

2. Check kernel module:
   ```bash
   lsmod | grep ch341
   ```

3. If not loaded:
   ```bash
   sudo modprobe ch341
   ```

## System Requirements

- **OS**: Linux (tested on Ubuntu)
- **Processing IDE**: 4.x
- **Bluetooth**: bluez, rfcomm
- **User groups**: dialout
