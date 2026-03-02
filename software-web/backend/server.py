#!/usr/bin/env python3
"""
Potyplex EEG - FastAPI Backend
Receives UDP data from ESP32 and broadcasts via WebSocket to web clients.
Supports video capture from USB cameras.

Usage:
    uvicorn server:app --reload --host 0.0.0.0 --port 8000
"""

import asyncio
import base64
import csv
import json
import os
import time
import threading
from contextlib import asynccontextmanager
from datetime import datetime
from pathlib import Path
from typing import Set, List, Optional, Dict

import cv2
from fastapi import FastAPI, WebSocket, WebSocketDisconnect, Query
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import FileResponse, JSONResponse

# Configuration
ESP32_IP = "192.168.4.1"
ESP32_UDP_PORT = 12345
LOCAL_UDP_PORT = 12346  # Fixed port for receiving
SAMPLE_RATE = 250  # Hz
BROADCAST_INTERVAL = 0.05  # 50ms = 20 batches/second

# File rotation settings
MAX_RECORDING_DURATION = 12 * 60 * 60  # 12 hours in seconds
MAX_RECORDING_SIZE_MB = 500  # Max file size before rotation (MB)

# Camera settings
CAMERA_FRAME_RATE = 15  # Target FPS for streaming
CAMERA_JPEG_QUALITY = 70  # JPEG compression quality (1-100)

# Connected WebSocket clients
clients: Set[WebSocket] = set()

# Data buffer for batching
data_buffer: List[dict] = []

# UDP transport (global)
udp_transport = None

# Streaming state
streaming_active = False

# Recording state
recording_active = False
recording_file: Optional[csv.writer] = None
recording_handle = None
recording_dir: Optional[Path] = None
recording_session_id: Optional[str] = None
recording_start_time: Optional[float] = None
recording_metadata: dict = {}
recording_part: int = 0  # Current recording part (for rotation)
recording_channels: List[int] = []  # Channels to record (0-7)
RECORDINGS_DIR = Path(__file__).parent / "recordings"
FLUSH_INTERVAL = 250  # Flush to disk every 250 samples (1 second)

# Camera state
camera_capture: Optional[cv2.VideoCapture] = None
camera_active = False
camera_recording = False
camera_writer: Optional[cv2.VideoWriter] = None
camera_clients: Set[WebSocket] = set()
camera_frame_count = 0
camera_lock = threading.Lock()

# Statistics
stats = {
    "samples_received": 0,
    "batches_sent": 0,
    "clients_connected": 0,
    "streaming": False,
    "last_sample_time": 0,
    "sample_rate": 0,
    "recording": False,
    "recording_samples": 0,
    "recording_file": None,
    "camera_active": False,
    "camera_recording": False,
    "camera_fps": 0
}

# System event log (circular buffer)
MAX_LOG_ENTRIES = 100
system_logs: List[dict] = []

def add_log(event: str, level: str = "info"):
    """Add entry to system log."""
    entry = {
        "time": datetime.now().isoformat(),
        "level": level,
        "event": event
    }
    system_logs.append(entry)
    if len(system_logs) > MAX_LOG_ENTRIES:
        system_logs.pop(0)
    print(f"[{level.upper()}] {event}")


def detect_cameras() -> List[Dict]:
    """Detect available USB cameras and their resolutions."""
    cameras = []

    # Try camera indices 0-9
    for idx in range(10):
        cap = cv2.VideoCapture(idx)
        if cap.isOpened():
            # Get camera properties
            width = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
            height = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
            fps = cap.get(cv2.CAP_PROP_FPS) or 30

            # Try to get camera name (may not work on all systems)
            name = f"Camera {idx}"

            # Test common resolutions
            resolutions = []
            test_res = [(640, 480), (1280, 720), (1920, 1080), (320, 240)]
            for w, h in test_res:
                cap.set(cv2.CAP_PROP_FRAME_WIDTH, w)
                cap.set(cv2.CAP_PROP_FRAME_HEIGHT, h)
                actual_w = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
                actual_h = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
                res = f"{actual_w}x{actual_h}"
                if res not in resolutions:
                    resolutions.append(res)

            cameras.append({
                "index": idx,
                "name": name,
                "current_resolution": f"{width}x{height}",
                "supported_resolutions": resolutions,
                "fps": fps
            })
            cap.release()

    return cameras


def start_camera(camera_index: int = 0, width: int = 640, height: int = 480) -> bool:
    """Start camera capture."""
    global camera_capture, camera_active

    with camera_lock:
        if camera_capture is not None:
            camera_capture.release()

        camera_capture = cv2.VideoCapture(camera_index)
        if not camera_capture.isOpened():
            add_log(f"Failed to open camera {camera_index}", "error")
            return False

        # Set resolution
        camera_capture.set(cv2.CAP_PROP_FRAME_WIDTH, width)
        camera_capture.set(cv2.CAP_PROP_FRAME_HEIGHT, height)
        camera_capture.set(cv2.CAP_PROP_FPS, CAMERA_FRAME_RATE)

        # Reduce buffer size for lower latency
        camera_capture.set(cv2.CAP_PROP_BUFFERSIZE, 1)

        camera_active = True
        stats["camera_active"] = True

        actual_w = int(camera_capture.get(cv2.CAP_PROP_FRAME_WIDTH))
        actual_h = int(camera_capture.get(cv2.CAP_PROP_FRAME_HEIGHT))
        add_log(f"Camera {camera_index} started at {actual_w}x{actual_h}")
        return True


def stop_camera():
    """Stop camera capture."""
    global camera_capture, camera_active, camera_writer, camera_recording

    with camera_lock:
        camera_active = False
        stats["camera_active"] = False

        if camera_writer is not None:
            camera_writer.release()
            camera_writer = None
            camera_recording = False
            stats["camera_recording"] = False

        if camera_capture is not None:
            camera_capture.release()
            camera_capture = None

        add_log("Camera stopped")


def get_camera_frame() -> Optional[bytes]:
    """Capture a frame and encode as JPEG."""
    global camera_frame_count

    if not camera_active or camera_capture is None:
        return None

    with camera_lock:
        ret, frame = camera_capture.read()
        if not ret:
            return None

        # Encode as JPEG
        encode_params = [cv2.IMWRITE_JPEG_QUALITY, CAMERA_JPEG_QUALITY]
        _, buffer = cv2.imencode('.jpg', frame, encode_params)
        camera_frame_count += 1

        return buffer.tobytes()


def start_camera_recording(output_path: Path, fps: int = 15) -> bool:
    """Start recording video to file."""
    global camera_writer, camera_recording

    if not camera_active or camera_capture is None:
        return False

    with camera_lock:
        width = int(camera_capture.get(cv2.CAP_PROP_FRAME_WIDTH))
        height = int(camera_capture.get(cv2.CAP_PROP_FRAME_HEIGHT))

        # Use MJPG codec for better compatibility
        fourcc = cv2.VideoWriter_fourcc(*'MJPG')
        camera_writer = cv2.VideoWriter(str(output_path), fourcc, fps, (width, height))

        if not camera_writer.isOpened():
            add_log("Failed to start video recording", "error")
            return False

        camera_recording = True
        stats["camera_recording"] = True
        add_log(f"Video recording started: {output_path.name}")
        return True


def stop_camera_recording():
    """Stop video recording."""
    global camera_writer, camera_recording

    with camera_lock:
        if camera_writer is not None:
            camera_writer.release()
            camera_writer = None
        camera_recording = False
        stats["camera_recording"] = False
        add_log("Video recording stopped")


def write_camera_frame():
    """Write current frame to video file if recording."""
    if not camera_recording or camera_writer is None or camera_capture is None:
        return

    with camera_lock:
        ret, frame = camera_capture.read()
        if ret:
            camera_writer.write(frame)


def check_recording_rotation() -> bool:
    """Check if recording file needs rotation and perform it if necessary."""
    global recording_file, recording_handle, recording_part, recording_metadata

    if not recording_active or not recording_dir:
        return False

    needs_rotation = False
    reason = ""

    # Check duration
    elapsed = time.time() - recording_start_time
    if elapsed >= MAX_RECORDING_DURATION:
        needs_rotation = True
        reason = "max duration reached"

    # Check file size
    csv_path = recording_dir / f"data_part{recording_part}.csv" if recording_part > 0 else recording_dir / "data.csv"
    if csv_path.exists():
        size_mb = csv_path.stat().st_size / (1024 * 1024)
        if size_mb >= MAX_RECORDING_SIZE_MB:
            needs_rotation = True
            reason = f"max size reached ({size_mb:.1f} MB)"

    if not needs_rotation:
        return False

    # Perform rotation
    add_log(f"Rotating recording file: {reason}")

    # Save current part metadata
    part_end_time = datetime.now().isoformat()
    part_samples = stats["recording_samples"]

    # Close current file
    if recording_handle:
        recording_handle.flush()
        recording_handle.close()

    # Increment part number
    recording_part += 1

    # Create new CSV file
    csv_path = recording_dir / f"data_part{recording_part}.csv"
    recording_handle = open(csv_path, 'w', newline='')
    recording_file = csv.writer(recording_handle)

    # Write header
    recording_file.writerow([
        'sample_index', 'time_s', 'packet_num',
        'ch1_uv', 'ch2_uv', 'ch3_uv', 'ch4_uv',
        'ch5_uv', 'ch6_uv', 'ch7_uv', 'ch8_uv',
        'accel_x', 'accel_y', 'accel_z'
    ])

    # Update metadata with parts info
    if "parts" not in recording_metadata:
        recording_metadata["parts"] = []

    recording_metadata["parts"].append({
        "part": recording_part - 1,
        "filename": f"data_part{recording_part - 1}.csv" if recording_part > 1 else "data.csv",
        "end_time": part_end_time,
        "samples": part_samples
    })

    add_log(f"Recording rotated to part {recording_part}")
    return True


def parse_eeg_packet(data: bytes) -> dict | None:
    """Parse OpenBCI packet format (33 bytes)."""
    if len(data) != 33:
        return None

    if data[0] != 0xA0 or data[32] != 0xC0:
        return None

    sample_num = data[1]

    # Parse 8 EEG channels (24-bit signed, big-endian)
    channels = []
    for ch in range(8):
        offset = 2 + ch * 3
        raw = (data[offset] << 16) | (data[offset + 1] << 8) | data[offset + 2]
        if raw & 0x800000:
            raw -= 0x1000000
        # Convert to microvolts (gain 24, Vref 4.5V)
        uv = raw * 4.5 / (24 * 8388607) * 1000000
        channels.append(round(uv, 2))

    # Parse accelerometer (16-bit signed)
    accel = []
    for i in range(3):
        offset = 26 + i * 2
        raw = (data[offset] << 8) | data[offset + 1]
        if raw & 0x8000:
            raw -= 0x10000
        accel.append(raw)

    return {
        "s": sample_num,
        "c": channels,
        "a": accel
    }


class EEGProtocol(asyncio.DatagramProtocol):
    """UDP Protocol for receiving EEG data."""

    def __init__(self):
        self.transport = None

    def connection_made(self, transport):
        self.transport = transport
        global udp_transport
        udp_transport = transport
        sockname = transport.get_extra_info('sockname')
        print(f"[UDP] Listening on {sockname[0]}:{sockname[1]}")

    def datagram_received(self, data, addr):
        global recording_file

        packet = parse_eeg_packet(data)
        if packet:
            stats["samples_received"] += 1
            stats["streaming"] = True
            stats["last_sample_time"] = time.time()
            data_buffer.append(packet)

            # Record to file if recording is active
            if recording_active and recording_file:
                elapsed = time.time() - recording_start_time
                row = [
                    stats["recording_samples"],
                    round(elapsed, 4),
                    packet['s'],
                ]
                # Add only selected channels
                for ch in recording_channels:
                    row.append(packet['c'][ch])
                row.extend(packet['a'])  # 3 accel axes
                recording_file.writerow(row)
                stats["recording_samples"] += 1

                # Periodic flush to prevent data loss
                if stats["recording_samples"] % FLUSH_INTERVAL == 0:
                    recording_handle.flush()
                    # Check if rotation is needed
                    check_recording_rotation()

            # Debug: log every 250 samples (once per second)
            if stats["samples_received"] % 250 == 0:
                ch = packet['c']
                print(f"[DATA] Sample {packet['s']:3d}: "
                      f"CH1={ch[0]:8.1f} CH2={ch[1]:8.1f} CH3={ch[2]:8.1f} CH4={ch[3]:8.1f} | "
                      f"CH5={ch[4]:8.1f} CH6={ch[5]:8.1f} CH7={ch[6]:8.1f} CH8={ch[7]:8.1f} uV")

    def error_received(self, exc):
        print(f"[UDP] Error: {exc}")


async def broadcast_worker():
    """Worker that broadcasts batched data to WebSocket clients at fixed intervals."""
    global data_buffer

    while True:
        try:
            await asyncio.sleep(BROADCAST_INTERVAL)

            if not clients or not data_buffer:
                continue

            # Swap buffer
            batch = data_buffer
            data_buffer = []

            # Create batch message
            message = json.dumps({"type": "batch", "samples": batch})

            # Send to all clients
            disconnected = set()
            for client in clients.copy():
                try:
                    await client.send_text(message)
                except Exception:
                    disconnected.add(client)

            for client in disconnected:
                clients.discard(client)
                stats["clients_connected"] = len(clients)

            stats["batches_sent"] += 1

        except Exception as e:
            print(f"[Broadcast] Error: {e}")
            await asyncio.sleep(0.1)


async def stats_worker():
    """Calculate and print statistics."""
    last_samples = 0
    last_time = time.time()

    while True:
        await asyncio.sleep(2)

        current_samples = stats["samples_received"]
        current_time = time.time()
        elapsed = current_time - last_time

        if elapsed > 0:
            rate = (current_samples - last_samples) / elapsed
            stats["sample_rate"] = round(rate)

        last_samples = current_samples
        last_time = current_time

        # Log stats
        print(f"[STATS] Rate: {stats['sample_rate']}/s, "
              f"Samples: {stats['samples_received']}, "
              f"Batches: {stats['batches_sent']}, "
              f"Clients: {stats['clients_connected']}")


async def camera_worker():
    """Worker that captures and broadcasts camera frames."""
    global camera_frame_count
    last_frame_count = 0
    last_time = time.time()
    frame_interval = 1.0 / CAMERA_FRAME_RATE

    while True:
        try:
            if not camera_active or not camera_clients:
                await asyncio.sleep(0.1)
                continue

            # Capture frame
            frame_data = get_camera_frame()
            if frame_data is None:
                await asyncio.sleep(0.05)
                continue

            # Write to video file if recording
            if camera_recording and camera_writer is not None:
                write_camera_frame()

            # Encode as base64 for WebSocket
            frame_b64 = base64.b64encode(frame_data).decode('utf-8')
            message = json.dumps({
                "type": "camera_frame",
                "frame": frame_b64,
                "timestamp": time.time()
            })

            # Send to all camera clients
            disconnected = set()
            for client in camera_clients.copy():
                try:
                    await client.send_text(message)
                except Exception:
                    disconnected.add(client)

            for client in disconnected:
                camera_clients.discard(client)

            # Calculate FPS
            current_time = time.time()
            if current_time - last_time >= 1.0:
                stats["camera_fps"] = camera_frame_count - last_frame_count
                last_frame_count = camera_frame_count
                last_time = current_time

            # Wait for next frame
            await asyncio.sleep(frame_interval)

        except Exception as e:
            print(f"[Camera] Error: {e}")
            await asyncio.sleep(0.1)


@asynccontextmanager
async def lifespan(app: FastAPI):
    """Startup and shutdown events."""
    print("=" * 60)
    print("  Potyplex EEG - FastAPI Backend")
    print("=" * 60)
    print(f"  ESP32:       {ESP32_IP}:{ESP32_UDP_PORT}")
    print(f"  Local UDP:   0.0.0.0:{LOCAL_UDP_PORT}")
    print(f"  Batch Rate:  {1/BROADCAST_INTERVAL:.0f} Hz")
    print(f"  API:         http://localhost:8000")
    print(f"  WebSocket:   ws://localhost:8000/ws")
    print("=" * 60)

    loop = asyncio.get_event_loop()

    # Create UDP endpoint on fixed port
    transport, protocol = await loop.create_datagram_endpoint(
        lambda: EEGProtocol(),
        local_addr=('0.0.0.0', LOCAL_UDP_PORT)
    )

    # Start background tasks
    broadcast_task = asyncio.create_task(broadcast_worker())
    stats_task = asyncio.create_task(stats_worker())
    camera_task = asyncio.create_task(camera_worker())

    add_log("Server started")

    yield

    # Cleanup
    broadcast_task.cancel()
    stats_task.cancel()
    camera_task.cancel()
    stop_camera()
    transport.close()


app = FastAPI(
    title="Potyplex EEG API",
    description="Real-time EEG data streaming",
    version="1.0.0",
    lifespan=lifespan
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)


@app.get("/")
async def root():
    return {
        "name": "Potyplex EEG API",
        "version": "1.0.0",
        "websocket": "/ws",
        "stats": "/stats"
    }


@app.get("/stats")
async def get_stats():
    return stats


@app.get("/logs")
async def get_logs():
    """Get system event logs."""
    return {"logs": system_logs}


@app.post("/start")
async def start_streaming():
    global streaming_active
    if udp_transport:
        udp_transport.sendto(b'b', (ESP32_IP, ESP32_UDP_PORT))
        streaming_active = True
        stats["streaming"] = True
        add_log("Streaming started via REST API")
        return {"status": "ok", "message": "Streaming started"}
    add_log("Failed to start streaming - UDP not ready", "error")
    return {"status": "error", "message": "UDP not ready"}


@app.post("/stop")
async def stop_streaming():
    global streaming_active
    if udp_transport:
        udp_transport.sendto(b's', (ESP32_IP, ESP32_UDP_PORT))
        streaming_active = False
        stats["streaming"] = False
        add_log("Streaming stopped via REST API")
        return {"status": "ok", "message": "Streaming stopped"}
    add_log("Failed to stop streaming - UDP not ready", "error")
    return {"status": "error", "message": "UDP not ready"}


@app.post("/record/start")
async def start_recording(subject: str = "", notes: str = "", include_video: bool = True, channels: str = "0,1,2,3,4,5,6,7"):
    """Start recording EEG data to CSV file with metadata."""
    global recording_active, recording_file, recording_handle, recording_dir
    global recording_session_id, recording_start_time, recording_metadata, recording_part
    global recording_channels

    if recording_active:
        return {"status": "error", "message": "Already recording"}

    # Parse channels parameter (comma-separated indices)
    try:
        recording_channels = [int(ch.strip()) for ch in channels.split(',') if ch.strip()]
        recording_channels = [ch for ch in recording_channels if 0 <= ch <= 7]
        if not recording_channels:
            recording_channels = list(range(8))
    except ValueError:
        recording_channels = list(range(8))

    # Create recordings directory
    RECORDINGS_DIR.mkdir(exist_ok=True)

    # Generate session ID with timestamp
    timestamp = datetime.now()
    recording_session_id = timestamp.strftime("%Y%m%d_%H%M%S")
    recording_dir = RECORDINGS_DIR / recording_session_id
    recording_dir.mkdir(exist_ok=True)

    # Reset part counter
    recording_part = 0

    # Create CSV file
    csv_path = recording_dir / "data.csv"
    recording_handle = open(csv_path, 'w', newline='')
    recording_file = csv.writer(recording_handle)

    # Write header with only selected channels
    header = ['sample_index', 'time_s', 'packet_num']
    for ch in recording_channels:
        header.append(f'ch{ch+1}_uv')
    header.extend(['accel_x', 'accel_y', 'accel_z'])
    recording_file.writerow(header)

    # Initialize metadata
    recording_start_time = time.time()
    recording_metadata = {
        "session_id": recording_session_id,
        "start_time": timestamp.isoformat(),
        "sample_rate": SAMPLE_RATE,
        "channels": len(recording_channels),
        "channel_indices": recording_channels,
        "subject": subject,
        "notes": notes,
        "markers": [],
        "has_video": False
    }

    # Start video recording if camera is active
    if include_video and camera_active:
        video_path = recording_dir / "video.avi"
        if start_camera_recording(video_path, CAMERA_FRAME_RATE):
            recording_metadata["has_video"] = True
            recording_metadata["video_fps"] = CAMERA_FRAME_RATE

    recording_active = True
    stats["recording"] = True
    stats["recording_samples"] = 0
    stats["recording_file"] = recording_session_id

    add_log(f"Recording started: {recording_session_id}")
    return {
        "status": "ok",
        "message": "Recording started",
        "session_id": recording_session_id,
        "has_video": recording_metadata["has_video"]
    }


@app.post("/record/stop")
async def stop_recording():
    """Stop recording and save metadata."""
    global recording_active, recording_file, recording_handle, recording_dir
    global recording_session_id, recording_metadata, recording_part

    if not recording_active:
        return {"status": "error", "message": "Not recording"}

    recording_active = False
    stats["recording"] = False

    # Stop video recording if active
    if camera_recording:
        stop_camera_recording()
        # Get video file size if exists
        video_path = recording_dir / "video.avi"
        if video_path.exists():
            recording_metadata["video_size_mb"] = round(video_path.stat().st_size / (1024 * 1024), 2)

    # Close CSV file
    if recording_handle:
        recording_handle.flush()
        recording_handle.close()
        recording_handle = None
        recording_file = None

    # Update and save metadata
    duration = time.time() - recording_start_time
    samples = stats["recording_samples"]

    recording_metadata["end_time"] = datetime.now().isoformat()
    recording_metadata["duration_s"] = round(duration, 2)
    recording_metadata["total_samples"] = samples
    recording_metadata["total_parts"] = recording_part + 1

    # Write metadata JSON
    if recording_dir:
        metadata_path = recording_dir / "metadata.json"
        with open(metadata_path, 'w') as f:
            json.dump(recording_metadata, f, indent=2)

    session_id = recording_session_id
    add_log(f"Recording stopped: {session_id} ({samples} samples, {duration:.1f}s)")

    return {
        "status": "ok",
        "message": "Recording stopped",
        "session_id": session_id,
        "samples": samples,
        "duration_s": round(duration, 2),
        "has_video": recording_metadata.get("has_video", False)
    }


@app.post("/record/marker")
async def add_marker(label: str = "marker"):
    """Add a marker/annotation to the current recording."""
    if not recording_active:
        return {"status": "error", "message": "Not recording"}

    elapsed = time.time() - recording_start_time
    marker = {
        "time_s": round(elapsed, 4),
        "sample": stats["recording_samples"],
        "label": label
    }
    recording_metadata["markers"].append(marker)

    add_log(f"Marker added: {label} @ {elapsed:.2f}s")
    return {"status": "ok", "marker": marker}


@app.get("/recordings")
async def list_recordings():
    """List all recording sessions with metadata."""
    RECORDINGS_DIR.mkdir(exist_ok=True)
    sessions = []

    # Look for session directories (new format)
    for d in sorted(RECORDINGS_DIR.iterdir(), reverse=True):
        if d.is_dir():
            metadata_path = d / "metadata.json"
            if metadata_path.exists():
                with open(metadata_path) as f:
                    metadata = json.load(f)
                csv_path = d / "data.csv"
                size_kb = round(csv_path.stat().st_size / 1024, 1) if csv_path.exists() else 0
                sessions.append({
                    "session_id": d.name,
                    "size_kb": size_kb,
                    **metadata
                })

    # Also check for legacy CSV files (old format)
    for f in sorted(RECORDINGS_DIR.glob("*.csv"), reverse=True):
        stat = f.stat()
        sessions.append({
            "session_id": f.stem,
            "filename": f.name,
            "size_kb": round(stat.st_size / 1024, 1),
            "start_time": datetime.fromtimestamp(stat.st_mtime).isoformat(),
            "legacy": True
        })

    return {"recordings": sessions}


@app.get("/recordings/{session_id}")
async def get_recording(session_id: str):
    """Get recording metadata and data."""
    # Check new format (directory)
    session_dir = RECORDINGS_DIR / session_id
    if session_dir.is_dir():
        metadata_path = session_dir / "metadata.json"
        csv_path = session_dir / "data.csv"

        if not metadata_path.exists():
            return {"status": "error", "message": "Metadata not found"}

        with open(metadata_path) as f:
            metadata = json.load(f)

        # Read CSV data
        data = []
        if csv_path.exists():
            with open(csv_path, 'r') as f:
                reader = csv.DictReader(f)
                # Get channel columns dynamically from header
                fieldnames = reader.fieldnames
                channel_cols = [col for col in fieldnames if col.endswith('_uv') and col.startswith('ch')]

                for row in reader:
                    # Read only available channels, pad with 0 for missing
                    channels = [0.0] * 8
                    for col in channel_cols:
                        ch_idx = int(col[2]) - 1  # 'ch1_uv' -> 0
                        if 0 <= ch_idx < 8:
                            channels[ch_idx] = float(row[col])

                    data.append({
                        "sample": int(row['sample_index']),
                        "time": float(row['time_s']),
                        "channels": channels
                    })

        return {
            "status": "ok",
            "metadata": metadata,
            "data": data
        }

    # Check legacy format (single CSV file)
    csv_path = RECORDINGS_DIR / f"{session_id}.csv"
    if csv_path.exists():
        data = []
        with open(csv_path, 'r') as f:
            reader = csv.DictReader(f)
            for row in reader:
                data.append({
                    "sample": int(row['sample_index']),
                    "time": float(row['time_s']),
                    "channels": [
                        float(row['ch1_uv']), float(row['ch2_uv']),
                        float(row['ch3_uv']), float(row['ch4_uv']),
                        float(row['ch5_uv']), float(row['ch6_uv']),
                        float(row['ch7_uv']), float(row['ch8_uv'])
                    ]
                })

        return {
            "status": "ok",
            "metadata": {"session_id": session_id, "legacy": True},
            "data": data
        }

    return {"status": "error", "message": "Recording not found"}


@app.get("/recordings/{session_id}/download")
async def download_recording(session_id: str):
    """Download recording CSV file."""
    # Check new format
    csv_path = RECORDINGS_DIR / session_id / "data.csv"
    if csv_path.exists():
        return FileResponse(csv_path, media_type="text/csv", filename=f"{session_id}.csv")

    # Check legacy format
    csv_path = RECORDINGS_DIR / f"{session_id}.csv"
    if csv_path.exists():
        return FileResponse(csv_path, media_type="text/csv", filename=f"{session_id}.csv")

    return {"status": "error", "message": "Recording not found"}


@app.get("/recordings/{session_id}/video")
async def get_recording_video(session_id: str):
    """Stream recording video file."""
    video_path = RECORDINGS_DIR / session_id / "video.avi"
    if video_path.exists():
        return FileResponse(video_path, media_type="video/x-msvideo", filename=f"{session_id}.avi")

    return JSONResponse(status_code=404, content={"status": "error", "message": "Video not found"})


@app.delete("/recordings/{session_id}")
async def delete_recording(session_id: str):
    """Delete a recording session."""
    import shutil

    # Check new format (directory)
    session_dir = RECORDINGS_DIR / session_id
    if session_dir.is_dir():
        shutil.rmtree(session_dir)
        add_log(f"Recording deleted: {session_id}")
        return {"status": "ok", "message": "Recording deleted"}

    # Check legacy format
    csv_path = RECORDINGS_DIR / f"{session_id}.csv"
    if csv_path.exists():
        csv_path.unlink()
        add_log(f"Recording deleted: {session_id}")
        return {"status": "ok", "message": "Recording deleted"}

    return {"status": "error", "message": "Recording not found"}


# ============== Camera Endpoints ==============

@app.get("/cameras")
async def list_cameras():
    """List available USB cameras."""
    cameras = detect_cameras()
    return {
        "cameras": cameras,
        "active": camera_active,
        "recording": camera_recording
    }


@app.post("/camera/start")
async def api_start_camera(
    camera_index: int = 0,
    width: int = 640,
    height: int = 480
):
    """Start camera capture."""
    if camera_active:
        return {"status": "error", "message": "Camera already active"}

    success = start_camera(camera_index, width, height)
    if success:
        return {"status": "ok", "message": "Camera started"}
    return {"status": "error", "message": "Failed to start camera"}


@app.post("/camera/stop")
async def api_stop_camera():
    """Stop camera capture."""
    stop_camera()
    return {"status": "ok", "message": "Camera stopped"}


@app.get("/camera/snapshot")
async def camera_snapshot():
    """Get a single camera frame as JPEG."""
    if not camera_active:
        return JSONResponse(
            status_code=400,
            content={"status": "error", "message": "Camera not active"}
        )

    frame = get_camera_frame()
    if frame is None:
        return JSONResponse(
            status_code=500,
            content={"status": "error", "message": "Failed to capture frame"}
        )

    from fastapi.responses import Response
    return Response(content=frame, media_type="image/jpeg")


@app.websocket("/ws/camera")
async def camera_websocket(websocket: WebSocket):
    """WebSocket endpoint for camera streaming."""
    await websocket.accept()
    camera_clients.add(websocket)

    client_ip = websocket.client.host if websocket.client else "unknown"
    add_log(f"Camera client connected: {client_ip}")

    # Send initial status
    await websocket.send_json({
        "type": "camera_status",
        "active": camera_active,
        "recording": camera_recording
    })

    try:
        while True:
            # Wait for messages (camera control commands)
            try:
                data = await asyncio.wait_for(websocket.receive_text(), timeout=30)
                cmd = json.loads(data)

                if cmd.get("action") == "start":
                    idx = cmd.get("camera_index", 0)
                    w = cmd.get("width", 640)
                    h = cmd.get("height", 480)
                    success = start_camera(idx, w, h)
                    await websocket.send_json({
                        "type": "camera_status",
                        "active": success,
                        "recording": camera_recording
                    })

                elif cmd.get("action") == "stop":
                    stop_camera()
                    await websocket.send_json({
                        "type": "camera_status",
                        "active": False,
                        "recording": False
                    })

            except asyncio.TimeoutError:
                # Send keepalive
                try:
                    await websocket.send_json({"type": "ping"})
                except Exception:
                    break

    except WebSocketDisconnect:
        pass
    except Exception as e:
        print(f"[Camera WS] Error: {e}")
    finally:
        camera_clients.discard(websocket)
        add_log(f"Camera client disconnected: {client_ip}")


@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    global streaming_active

    await websocket.accept()
    clients.add(websocket)
    stats["clients_connected"] = len(clients)

    client_ip = websocket.client.host if websocket.client else "unknown"
    add_log(f"WebSocket client connected: {client_ip} (total: {len(clients)})")

    # Send initial config
    try:
        await websocket.send_json({
            "type": "config",
            "sample_rate": SAMPLE_RATE,
            "channels": 8
        })
    except Exception:
        clients.discard(websocket)
        return

    try:
        while True:
            try:
                data = await asyncio.wait_for(websocket.receive_text(), timeout=30)
                cmd = json.loads(data)

                if cmd.get("action") == "start":
                    if udp_transport:
                        udp_transport.sendto(b'b', (ESP32_IP, ESP32_UDP_PORT))
                        streaming_active = True
                        stats["streaming"] = True
                        add_log("Streaming started via WebSocket")
                        await websocket.send_json({"type": "status", "streaming": True})

                elif cmd.get("action") == "stop":
                    if udp_transport:
                        udp_transport.sendto(b's', (ESP32_IP, ESP32_UDP_PORT))
                        streaming_active = False
                        stats["streaming"] = False
                        add_log("Streaming stopped via WebSocket")
                        await websocket.send_json({"type": "status", "streaming": False})

            except asyncio.TimeoutError:
                try:
                    await websocket.send_json({"type": "ping"})
                except Exception:
                    break

    except WebSocketDisconnect:
        pass
    except Exception as e:
        print(f"[WS] Error: {e}")
    finally:
        clients.discard(websocket)
        stats["clients_connected"] = len(clients)
        add_log(f"WebSocket client disconnected: {client_ip} (total: {len(clients)})")

        # Stop streaming if no more clients
        if not clients:
            streaming_active = False


if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
