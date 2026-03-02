"""
Tests for the EEG backend server.

Run with: pytest test_server.py -v
"""

import pytest
import struct
from unittest.mock import patch, MagicMock
from fastapi.testclient import TestClient


# Import the parse function and app
from server import parse_eeg_packet, app


# ============== Packet Parser Tests ==============

class TestParseEEGPacket:
    """Tests for the OpenBCI packet parser."""

    def create_valid_packet(self, sample_num=0, channels=None, accel=None):
        """Helper to create a valid 33-byte OpenBCI packet."""
        if channels is None:
            channels = [0] * 8
        if accel is None:
            accel = [0, 0, 0]

        packet = bytearray(33)
        packet[0] = 0xA0  # Header
        packet[1] = sample_num  # Sample number
        packet[32] = 0xC0  # Footer

        # Set channel data (24-bit signed big-endian)
        for i, ch_val in enumerate(channels):
            offset = 2 + i * 3
            # Convert to 24-bit signed
            if ch_val < 0:
                ch_val = ch_val + 0x1000000
            packet[offset] = (ch_val >> 16) & 0xFF
            packet[offset + 1] = (ch_val >> 8) & 0xFF
            packet[offset + 2] = ch_val & 0xFF

        # Set accelerometer data (16-bit signed big-endian)
        for i, acc_val in enumerate(accel):
            offset = 26 + i * 2
            if acc_val < 0:
                acc_val = acc_val + 0x10000
            packet[offset] = (acc_val >> 8) & 0xFF
            packet[offset + 1] = acc_val & 0xFF

        return bytes(packet)

    def test_valid_packet_structure(self):
        """Test that a valid packet is parsed correctly."""
        packet = self.create_valid_packet(sample_num=42)
        result = parse_eeg_packet(packet)

        assert result is not None
        assert result['s'] == 42
        assert len(result['c']) == 8
        assert len(result['a']) == 3

    def test_invalid_packet_length(self):
        """Test that packets with wrong length are rejected."""
        assert parse_eeg_packet(b'') is None
        assert parse_eeg_packet(b'\x00' * 32) is None
        assert parse_eeg_packet(b'\x00' * 34) is None

    def test_invalid_header(self):
        """Test that packets with wrong header are rejected."""
        packet = bytearray(33)
        packet[0] = 0x00  # Wrong header
        packet[32] = 0xC0
        assert parse_eeg_packet(bytes(packet)) is None

    def test_invalid_footer(self):
        """Test that packets with wrong footer are rejected."""
        packet = bytearray(33)
        packet[0] = 0xA0
        packet[32] = 0x00  # Wrong footer
        assert parse_eeg_packet(bytes(packet)) is None

    def test_sample_number_range(self):
        """Test sample numbers from 0 to 255."""
        for sample_num in [0, 1, 127, 128, 255]:
            packet = self.create_valid_packet(sample_num=sample_num)
            result = parse_eeg_packet(packet)
            assert result['s'] == sample_num

    def test_channel_zero_values(self):
        """Test that zero channel values parse correctly."""
        packet = self.create_valid_packet(channels=[0] * 8)
        result = parse_eeg_packet(packet)

        for ch in result['c']:
            assert ch == 0.0

    def test_channel_positive_values(self):
        """Test positive channel values."""
        # Max positive 24-bit value
        packet = self.create_valid_packet(channels=[0x7FFFFF] * 8)
        result = parse_eeg_packet(packet)

        for ch in result['c']:
            assert ch > 0

    def test_channel_negative_values(self):
        """Test negative channel values (two's complement)."""
        # -1 in 24-bit two's complement = 0xFFFFFF
        packet = self.create_valid_packet(channels=[-1] * 8)
        result = parse_eeg_packet(packet)

        for ch in result['c']:
            assert ch < 0

    def test_microvolt_conversion(self):
        """Test that raw values are converted to microvolts correctly."""
        # Known conversion: raw * 4.5 / (24 * 8388607) * 1000000
        # For raw = 8388607 (max positive), should give ~22.35 uV
        packet = self.create_valid_packet(channels=[8388607, 0, 0, 0, 0, 0, 0, 0])
        result = parse_eeg_packet(packet)

        expected_uv = 8388607 * 4.5 / (24 * 8388607) * 1000000
        assert abs(result['c'][0] - expected_uv) < 0.01

    def test_accelerometer_values(self):
        """Test accelerometer parsing."""
        packet = self.create_valid_packet(accel=[100, -50, 980])
        result = parse_eeg_packet(packet)

        assert result['a'][0] == 100
        assert result['a'][1] == -50
        assert result['a'][2] == 980


# ============== API Endpoint Tests ==============

class TestAPIEndpoints:
    """Tests for FastAPI endpoints."""

    @pytest.fixture
    def client(self):
        """Create test client."""
        return TestClient(app)

    def test_root_endpoint(self, client):
        """Test root endpoint returns API info."""
        response = client.get("/")
        assert response.status_code == 200
        data = response.json()
        assert "name" in data or "status" in data

    def test_stats_endpoint(self, client):
        """Test stats endpoint returns statistics."""
        response = client.get("/stats")
        assert response.status_code == 200
        data = response.json()
        assert "samples_received" in data or "streaming" in data

    def test_recordings_list_endpoint(self, client):
        """Test recordings list endpoint."""
        response = client.get("/recordings")
        assert response.status_code == 200
        data = response.json()
        assert "recordings" in data
        assert isinstance(data["recordings"], list)

    def test_cameras_list_endpoint(self, client):
        """Test cameras list endpoint."""
        response = client.get("/cameras")
        assert response.status_code == 200
        data = response.json()
        assert "cameras" in data

    def test_recording_not_found(self, client):
        """Test getting non-existent recording."""
        response = client.get("/recordings/nonexistent_session_id")
        assert response.status_code == 200
        data = response.json()
        assert data.get("status") == "error"

    def test_video_not_found(self, client):
        """Test getting non-existent video."""
        response = client.get("/recordings/nonexistent_session_id/video")
        assert response.status_code == 404


# ============== Recording Logic Tests ==============

class TestRecordingLogic:
    """Tests for recording functionality."""

    @pytest.fixture
    def client(self):
        """Create test client."""
        return TestClient(app)

    def test_start_recording_without_sources(self, client):
        """Test that recording fails without any source."""
        response = client.post("/record/start?include_signals=false&include_video=false")
        assert response.status_code == 200
        data = response.json()
        assert data.get("status") == "error"
        assert "No recording source" in data.get("message", "")

    def test_stop_recording_when_not_recording(self, client):
        """Test stopping when not recording."""
        response = client.post("/record/stop")
        assert response.status_code == 200
        data = response.json()
        assert data.get("status") == "error"


# ============== WebSocket Tests ==============

class TestWebSocket:
    """Tests for WebSocket connections."""

    def test_websocket_connection(self):
        """Test WebSocket connection and initial config message."""
        client = TestClient(app)
        with client.websocket_connect("/ws") as websocket:
            # Should receive config message first
            data = websocket.receive_json()
            assert data["type"] == "config"
            assert "sample_rate" in data
            assert "channels" in data
            assert data["channels"] == 8


# ============== Integration Tests ==============

class TestIntegration:
    """Integration tests for the complete flow."""

    def test_packet_to_broadcast_flow(self):
        """Test that parsed packets have correct structure for broadcasting."""
        # Create a realistic packet
        packet_data = bytearray(33)
        packet_data[0] = 0xA0
        packet_data[1] = 123  # Sample number
        packet_data[32] = 0xC0

        # Set some channel values
        for i in range(8):
            offset = 2 + i * 3
            # Small positive value
            val = 1000 * (i + 1)
            packet_data[offset] = (val >> 16) & 0xFF
            packet_data[offset + 1] = (val >> 8) & 0xFF
            packet_data[offset + 2] = val & 0xFF

        result = parse_eeg_packet(bytes(packet_data))

        # Verify structure matches what frontend expects
        assert 's' in result  # sample number
        assert 'c' in result  # channels
        assert 'a' in result  # accelerometer
        assert len(result['c']) == 8
        assert all(isinstance(ch, float) for ch in result['c'])


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
