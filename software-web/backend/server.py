#!/usr/bin/env python3
"""
Potyplex EEG - FastAPI Backend
Receives UDP data from ESP32 and broadcasts via WebSocket to web clients.

Usage:
    uvicorn server:app --reload --host 0.0.0.0 --port 8000
"""

import asyncio
import json
import socket
from contextlib import asynccontextmanager
from typing import Set

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware

# Configuration
ESP32_IP = "192.168.4.1"
UDP_PORT = 12345
SAMPLE_RATE = 250  # Hz

# Connected WebSocket clients
clients: Set[WebSocket] = set()

# UDP socket (global)
udp_socket = None

# Statistics
stats = {
    "samples_received": 0,
    "packets_sent": 0,
    "clients_connected": 0,
    "streaming": False
}


def parse_eeg_packet(data: bytes) -> dict | None:
    """
    Parse OpenBCI packet format (33 bytes).
    """
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
        "sample": sample_num,
        "channels": channels,
        "accel": accel
    }


async def broadcast_to_clients(message: str):
    """Send message to all connected WebSocket clients."""
    if not clients:
        return

    disconnected = set()
    for client in clients:
        try:
            await client.send_text(message)
        except:
            disconnected.add(client)

    # Remove disconnected clients
    for client in disconnected:
        clients.discard(client)
        stats["clients_connected"] = len(clients)


async def udp_receiver():
    """Receive UDP packets from ESP32 and broadcast to WebSocket clients."""
    global udp_socket

    # Create UDP socket
    udp_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    udp_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    udp_socket.setblocking(False)
    udp_socket.bind(('0.0.0.0', 0))

    print(f"[UDP] Bound to port {udp_socket.getsockname()[1]}")
    print(f"[UDP] Ready to receive from {ESP32_IP}:{UDP_PORT}")

    loop = asyncio.get_event_loop()

    while True:
        try:
            # Non-blocking receive
            try:
                data, addr = await asyncio.wait_for(
                    loop.run_in_executor(None, lambda: udp_socket.recvfrom(64)),
                    timeout=0.1
                )
            except asyncio.TimeoutError:
                await asyncio.sleep(0.01)
                continue

            # Parse packet
            packet = parse_eeg_packet(data)
            if packet is None:
                continue

            stats["samples_received"] += 1
            stats["streaming"] = True

            # Broadcast to WebSocket clients
            if clients:
                message = json.dumps(packet)
                await broadcast_to_clients(message)
                stats["packets_sent"] += 1

        except Exception as e:
            if "WinError" not in str(e) and "BlockingIOError" not in str(e):
                print(f"[UDP] Error: {e}")
            await asyncio.sleep(0.01)


async def stats_printer():
    """Print statistics periodically."""
    while True:
        await asyncio.sleep(10)
        print(f"[STATS] Samples: {stats['samples_received']}, "
              f"Sent: {stats['packets_sent']}, "
              f"Clients: {stats['clients_connected']}")


@asynccontextmanager
async def lifespan(app: FastAPI):
    """Startup and shutdown events."""
    print("=" * 50)
    print("  Potyplex EEG - FastAPI Backend")
    print("=" * 50)
    print(f"  ESP32 IP:    {ESP32_IP}")
    print(f"  UDP Port:    {UDP_PORT}")
    print(f"  API:         http://localhost:8000")
    print(f"  WebSocket:   ws://localhost:8000/ws")
    print("=" * 50)

    # Start background tasks
    udp_task = asyncio.create_task(udp_receiver())
    stats_task = asyncio.create_task(stats_printer())

    yield

    # Cleanup
    udp_task.cancel()
    stats_task.cancel()
    if udp_socket:
        udp_socket.close()


# Create FastAPI app
app = FastAPI(
    title="Potyplex EEG API",
    description="Real-time EEG data streaming",
    version="1.0.0",
    lifespan=lifespan
)

# CORS middleware
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
    if udp_socket:
        udp_socket.sendto(b'b', (ESP32_IP, UDP_PORT))
        return {"status": "ok", "message": "Start command sent"}
    return {"status": "error", "message": "UDP socket not ready"}


@app.post("/stop")
async def stop_streaming():
    """Send stop command to ESP32."""
    if udp_socket:
        udp_socket.sendto(b's', (ESP32_IP, UDP_PORT))
        stats["streaming"] = False
        return {"status": "ok", "message": "Stop command sent"}
    return {"status": "error", "message": "UDP socket not ready"}


@app.websocket("/ws")
async def websocket_endpoint(websocket: WebSocket):
    """WebSocket endpoint for real-time EEG data."""
    await websocket.accept()
    clients.add(websocket)
    stats["clients_connected"] = len(clients)

    client_ip = websocket.client.host
    print(f"[WS] Client connected: {client_ip} (total: {len(clients)})")

    # Send initial config
    await websocket.send_json({
        "type": "config",
        "sample_rate": SAMPLE_RATE,
        "channels": 8
    })

    try:
        while True:
            # Handle incoming messages from client
            try:
                data = await asyncio.wait_for(websocket.receive_text(), timeout=30)
                cmd = json.loads(data)

                if cmd.get("action") == "start":
                    if udp_socket:
                        udp_socket.sendto(b'b', (ESP32_IP, UDP_PORT))
                        await websocket.send_json({"type": "status", "streaming": True})

                elif cmd.get("action") == "stop":
                    if udp_socket:
                        udp_socket.sendto(b's', (ESP32_IP, UDP_PORT))
                        await websocket.send_json({"type": "status", "streaming": False})

            except asyncio.TimeoutError:
                # Send keepalive
                await websocket.send_json({"type": "ping"})

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
