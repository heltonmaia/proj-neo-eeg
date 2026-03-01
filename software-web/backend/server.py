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

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware

# Configuration
ESP32_IP = "192.168.4.1"
ESP32_UDP_PORT = 12345
LOCAL_UDP_PORT = 12346  # Fixed port for receiving
SAMPLE_RATE = 250  # Hz
BROADCAST_INTERVAL = 0.05  # 50ms = 20 batches/second
DATA_TIMEOUT = 2.0  # Consider data stale after 2 seconds
RECONNECT_INTERVAL = 1.0  # Check for reconnect every 1 second

# Connected WebSocket clients
clients: Set[WebSocket] = set()

# Data buffer for batching
data_buffer: List[dict] = []

# UDP transport (global)
udp_transport = None

# Streaming state
streaming_active = False

# Statistics
stats = {
    "samples_received": 0,
    "batches_sent": 0,
    "clients_connected": 0,
    "streaming": False,
    "last_sample_time": 0,
    "sample_rate": 0,
    "reconnects": 0
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
        packet = parse_eeg_packet(data)
        if packet:
            stats["samples_received"] += 1
            stats["streaming"] = True
            stats["last_sample_time"] = time.time()
            data_buffer.append(packet)

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


async def reconnect_worker():
    """Monitor data flow and reconnect if data stops arriving."""
    global streaming_active

    while True:
        try:
            await asyncio.sleep(RECONNECT_INTERVAL)

            # Only act if streaming should be active and we have clients
            if streaming_active and clients and udp_transport:
                time_since_data = time.time() - stats["last_sample_time"]

                # If no data for DATA_TIMEOUT seconds, send reconnect
                if stats["last_sample_time"] > 0 and time_since_data > DATA_TIMEOUT:
                    print(f"[Reconnect] No data for {time_since_data:.1f}s, sending 'b'")
                    udp_transport.sendto(b'b', (ESP32_IP, ESP32_UDP_PORT))
                    stats["reconnects"] += 1

        except Exception as e:
            print(f"[Reconnect] Error: {e}")


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
              f"Clients: {stats['clients_connected']}, "
              f"Reconnects: {stats['reconnects']}")


@asynccontextmanager
async def lifespan(app: FastAPI):
    """Startup and shutdown events."""
    print("=" * 60)
    print("  Potyplex EEG - FastAPI Backend")
    print("=" * 60)
    print(f"  ESP32:       {ESP32_IP}:{ESP32_UDP_PORT}")
    print(f"  Local UDP:   0.0.0.0:{LOCAL_UDP_PORT}")
    print(f"  Batch Rate:  {1/BROADCAST_INTERVAL:.0f} Hz")
    print(f"  Reconnect:   after {DATA_TIMEOUT}s without data")
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
    reconnect_task = asyncio.create_task(reconnect_worker())
    stats_task = asyncio.create_task(stats_worker())

    yield

    # Cleanup
    broadcast_task.cancel()
    reconnect_task.cancel()
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
    return {
        "name": "Potyplex EEG API",
        "version": "1.0.0",
        "websocket": "/ws",
        "stats": "/stats"
    }


@app.get("/stats")
async def get_stats():
    return stats


@app.post("/start")
async def start_streaming():
    global streaming_active
    if udp_transport:
        udp_transport.sendto(b'b', (ESP32_IP, ESP32_UDP_PORT))
        streaming_active = True
        stats["streaming"] = True
        return {"status": "ok", "message": "Streaming started"}
    return {"status": "error", "message": "UDP not ready"}


@app.post("/stop")
async def stop_streaming():
    global streaming_active
    if udp_transport:
        udp_transport.sendto(b's', (ESP32_IP, ESP32_UDP_PORT))
        streaming_active = False
        stats["streaming"] = False
        return {"status": "ok", "message": "Streaming stopped"}
    return {"status": "error", "message": "UDP not ready"}


@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    global streaming_active

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
                data = await asyncio.wait_for(websocket.receive_text(), timeout=30)
                cmd = json.loads(data)

                if cmd.get("action") == "start":
                    if udp_transport:
                        udp_transport.sendto(b'b', (ESP32_IP, ESP32_UDP_PORT))
                        streaming_active = True
                        stats["streaming"] = True
                        await websocket.send_json({"type": "status", "streaming": True})

                elif cmd.get("action") == "stop":
                    if udp_transport:
                        udp_transport.sendto(b's', (ESP32_IP, ESP32_UDP_PORT))
                        streaming_active = False
                        stats["streaming"] = False
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
        print(f"[WS] Client disconnected: {client_ip} (total: {len(clients)})")

        # Stop streaming if no more clients
        if not clients:
            streaming_active = False


if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
