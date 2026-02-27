#!/bin/bash
#
# Open OpenBCI GUI in Processing IDE
#

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
SKETCH="$PROJECT_DIR/software/openbci-gui/OpenBCI_GUI/OpenBCI_GUI.pde"

PROCESSING_SCRIPT="/mnt/hd3/apps/processing/start-processing.sh"

echo "=== Opening OpenBCI GUI ==="
echo "Sketch: $SKETCH"
echo ""

if [ ! -x "$PROCESSING_SCRIPT" ]; then
    echo "[ERROR] Processing not found: $PROCESSING_SCRIPT"
    exit 1
fi

exec "$PROCESSING_SCRIPT" "$SKETCH"
