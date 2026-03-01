#!/usr/bin/env python3
"""
Potyplex EEG - FastAPI Backend
Receives UDP data from ESP32 and broadcasts via WebSocket to web clients.

Usage:
    uvicorn server:app --reload --host 0.0.0.0 --port 8000
"""

import asyncio
import json
import time
from contextlib import asynccontextmanager
from typing import Set, List
from collections import deque

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware

# Configuration
ESP32_IP = "192.168.4.1"
UDP_PORT = 12345
SAMPLE_RATE = 250  # Hz
BROADCAST_INTERVAL = 0.05  # 50ms = 20 batches/second

# Connected WebSocket clients
clients: Set[WebSocket] = set()

# Data buffer for batching
data_buffer: List[dict] = []
buffer_lock = asyncio.Lock()

# UDP transport (global)
udp_transport = None

# Statistics
stats = {
    "samples_received": 0,
    "batches_sent": 0,
    "clients_connected": 0,
    "streaming": False,
    "last_sample_time": 0,
    "queue_size": 0
}


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
        "s": sample_num,  # Shortened keys for less bandwidth
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
        print(f"[UDP] Ready on {transport.get_extra_info('sockname')}")

    def datagram_received(self, data, addr):
        packet = parse_eeg_packet(data)
        if packet:
            stats["samples_received"] += 1
            stats["streaming"] = True
            stats["last_sample_time"] = time.time()
            # Add to buffer (no await needed, just append)
            data_buffer.append(packet)

    def error_received(self, exc):
        print(f"[UDP] Error: {exc}")


async def broadcast_worker():
    """Worker that broadcasts batched data to WebSocket clients at fixed intervals."""
    global data_buffer

    while True:
        try:
            await asyncio.sleep(BROADCAST_INTERVAL)

            # Skip if no clients or no data
            if not clients or not data_buffer:
                continue

            # Swap buffer (fast, non-blocking)
            batch = data_buffer
            data_buffer = []

            stats["queue_size"] = len(batch)

            # Create batch message
            message = json.dumps({"type": "batch", "samples": batch})

            # Send to all clients
            disconnected = set()
            for client in clients.copy():
                try:
                    await client.send_text(message)
                except Exception:
                    disconnected.add(client)

            # Remove disconnected clients
            for client in disconnected:
                clients.discard(client)
                stats["clients_connected"] = len(clients)

            stats["batches_sent"] += 1

        except Exception as e:
            print(f"[Broadcast] Error: {e}")
            await asyncio.sleep(0.1)


async def stats_printer():
    """Print statistics periodically."""
    last_samples = 0
    last_batches = 0
    while True:
        await asyncio.sleep(5)
        current_samples = stats["samples_received"]
        current_batches = stats["batches_sent"]
        sample_rate = (current_samples - last_samples) / 5
        batch_rate = (current_batches - last_batches) / 5
        last_samples = current_samples
        last_batches = current_batches
        print(f"[STATS] Samples: {current_samples} ({sample_rate:.0f}/s), "
              f"Batches: {current_batches} ({batch_rate:.0f}/s), "
              f"Clients: {stats['clients_connected']}")


@asynccontextmanager
async def lifespan(app: FastAPI):
    """Startup and shutdown events."""
    print("=" * 50)
    print("  Potyplex EEG - FastAPI Backend")
    print("=" * 50)
    print(f"  ESP32 IP:    {ESP32_IP}")
    print(f"  UDP Port:    {UDP_PORT}")
    print(f"  Batch Rate:  {1/BROADCAST_INTERVAL:.0f} Hz")
    print(f"  API:         http://localhost:8000")
    print(f"  WebSocket:   ws://localhost:8000/ws")
    print("=" * 50)

    loop = asyncio.get_event_loop()

    # Create UDP endpoint
    transport, protocol = await loop.create_datagram_endpoint(
        lambda: EEGProtocol(),
        local_addr=('0.0.0.0', 0)
    )

    # Start background tasks
    broadcast_task = asyncio.create_task(broadcast_worker())
    stats_task = asyncio.create_task(stats_printer())

    yield

    # Cleanup
    broadcast_task.cancel()
    stats_task.cancel()
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
    """API info."""
    return {
        "name": "Potyplex EEG API",
        "version": "1.0.0",
        "websocket": "/ws",
        "stats": "/stats"
    }


@app.get("/stats")
async def get_stats():
    """Get current statistics."""
    return stats


@app.post("/start")
async def start_streaming():
    """Send start command to ESP32."""
    if udp_transport:
        udp_transport.sendto(b'b', (ESP32_IP, UDP_PORT))
        return {"status": "ok", "message": "Start command sent"}
    return {"status": "error", "message": "UDP not ready"}


@app.post("/stop")
async def stop_streaming():
    """Send stop command to ESP32."""
    if udp_transport:
        udp_transport.sendto(b's', (ESP32_IP, UDP_PORT))
        stats["streaming"] = False
        return {"status": "ok", "message": "Stop command sent"}
    return {"status": "error", "message": "UDP not ready"}


@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    """WebSocket endpoint for real-time EEG data."""
    await websocket.accept()
    clients.add(websocket)
    stats["clients_connected"] = len(clients)

    client_ip = websocket.client.host if websocket.client else "unknown"
    print(f"[WS] Client connected: {client_ip} (total: {len(clients)})")

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
                # Wait for messages with timeout
                data = await asyncio.wait_for(websocket.receive_text(), timeout=30)
                cmd = json.loads(data)

                if cmd.get("action") == "start":
                    if udp_transport:
                        udp_transport.sendto(b'b', (ESP32_IP, UDP_PORT))
                        await websocket.send_json({"type": "status", "streaming": True})

                elif cmd.get("action") == "stop":
                    if udp_transport:
                        udp_transport.sendto(b's', (ESP32_IP, UDP_PORT))
                        await websocket.send_json({"type": "status", "streaming": False})

            except asyncio.TimeoutError:
                # Send ping to keep connection alive
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
        print(f"[WS] Client disconnected: {client_ip} (total: {len(clients)})")


if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
