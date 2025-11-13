#!/usr/bin/env python3
"""
Meshtastic Flipper Zero Monitor

Live TUI viewer for Meshtastic LoRa packets captured via Flipper Zero LoRa app.
Decrypts and displays text messages (including default channel 0).

Requirements:
  pip install textual rich cryptography meshtastic

Usage:
  python monitor.py [-p PORT] [-baud BAUDRATE]

Options:
  -p, --port       Serial port (e.g. /dev/ttyACM0 or COM3)
  -baud, --baudrate Baud rate (default: 115200)

Press 'q' to quit.

Note: Use Flipper Zero LoRa app in "Sniffer" mode.
"""
import serial
from serial.tools import list_ports
import argparse
import asyncio
import base64
import queue
import sys
import threading
import time
from dataclasses import dataclass
from datetime import datetime
from typing import Dict, Optional, Tuple

from cryptography.hazmat.backends import default_backend
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from rich.markup import escape as rich_escape
from rich.text import Text
from textual.app import App, ComposeResult
from textual.binding import Binding
from textual.containers import Container
from textual.widgets import DataTable, Static, Footer

from meshtastic import mesh_pb2

DEFAULT_BAUDRATE = 115200
DEFAULT_CRLF = "\r\n"
DEFAULT_COMPORT = "/dev/ttyACM0"


def find_serial_port():
    ports = list_ports.comports()
    for port in ports:
        return port.device
    return DEFAULT_COMPORT


class SerialError(Exception):
    pass


class serialDevice:
    def __init__(self, port=find_serial_port(), baudrate=DEFAULT_BAUDRATE):
        if port is None:
            raise SerialError("Serial port is None")
        if baudrate is None:
            baudrate = DEFAULT_BAUDRATE
        self.serial_device = serial.Serial(port=port, baudrate=baudrate)
        self.serial_line_control = DEFAULT_CRLF
        self.serial_alive = False

    def set_serial_port(self, port):
        self.serial_device.port = port

    def set_baudrate(self, baudrate):
        if baudrate not in serial.Serial.BAUDRATES:
            raise ValueError("Invalid baudrate")
        self.serial_device.baudrate = baudrate

    def get_port(self):
        return self.serial_device.port

    def is_connected(self):
        return self.serial_device.is_open

    def resetBuffer(self):
        self.serial_device.reset_input_buffer()
        self.serial_device.reset_output_buffer()

    def validate_connection(self):
        try:
            self.serial_device.open()
            self.close()
            return True
        except (serial.SerialException, FileNotFoundError):
            return False

    def open(self):
        try:
            if self.is_connected():
                self.close()

            self.serial_device.open()

            self.resetBuffer()
            self.serial_alive = True
        except serial.SerialException as e:
            raise SerialError(e)

    def close(self):
        if self.is_connected():
            try:
                self.serial_device.close()
                self.serial_alive = False
            except serial.SerialException as e:
                raise SerialError(e)

    def recv(self):
        try:
            if self.serial_device.in_waiting > 0:
                bytestream = self.serial_device.readline()
                return bytestream
        except (serial.SerialException, UnicodeDecodeError):
            self.serial_alive = False
            return None

    def transmit(self, data):
        try:
            message = f"{data}{self.serial_line_control}"
            self.serial_device.write(message.encode())
        except serial.SerialException as e:
            print(f"Error: {e}")

    def reconnect(self, max_attempts=5):
        attempts = 0
        while not self.serial_alive and attempts < max_attempts:
            time.sleep(3)
            try:
                print("Reconnecting")
                self.open()
                return
            except SerialError:
                attempts += 1
        if not self.serial_alive:
            print("No connected")
        else:
            print("Connected")


# ==============================
# TUI Meshtastic
# ==============================

DEFAULT_KEYS = [
    "1PG7OiApB1nwvP+rz05pAQ==",  # Channel 0 (default)
    "OEu8wB3AItGBvza4YSHh+5a3LlW/dCJ+nWr7SNZMsaE=",
    "6IzsaoVhx1ETWeWuu0dUWMLqItvYJLbRzwgTAKCfvtY=",
    "TiIdi8MJG+IRnIkS8iUZXRU+MHuGtuzEasOWXp4QndU=",
    "pHRzd/zw0r5DLAQavv84XfU2iLzjk3HczbdIXkgt5Mk=",
]


def extract_frame(raw: bytes) -> bytes:
    if not raw.startswith(b"@S") or not raw.endswith(b"@E\r\n"):
        raise ValueError("Invalid frame")
    length = int.from_bytes(raw[2:4], "big")
    return raw[4:4 + length]


def extract_fields(data: bytes) -> Dict[str, bytes]:
    return {
        "dest": data[0:4],
        "sender": data[4:8],
        "packet_id": data[8:12],
        "payload": data[16:],
    }


def decrypt(payload: bytes, key: bytes, sender: bytes, packet_id: bytes) -> bytes:
    nonce = packet_id + b"\x00\x00\x00\x00" + sender + b"\x00\x00\x00\x00"
    cipher = Cipher(algorithms.AES(key), modes.CTR(nonce), backend=default_backend())
    return cipher.decryptor().update(payload)


def sanitize_text(s: str) -> str:
    cleaned = []
    for ch in s:
        if ch in "\r":
            continue
        if ch == "\n" or ch == "\t" or ord(ch) >= 32:
            cleaned.append(ch)
        else:
            cleaned.append("�")
    return rich_escape("".join(cleaned))


@dataclass
class ChatMessage:
    ts: float
    sender_name: str
    text: str

    def as_row(self) -> Tuple[str, str, str, str]:
        return (
            datetime.fromtimestamp(self.ts).strftime("%H:%M:%S"),
            self.sender_name,
            self.text,
        )


class NodeRegistry:
    def __init__(self) -> None:
        self.nodes: Dict[str, str] = {}

    def set_from_user_pb(self, user: mesh_pb2.User) -> None:
        node_id = user.id or ""
        name = user.long_name or user.short_name or node_id
        self.nodes[node_id] = sanitize_text(name)

    def resolve(self, node_id: str) -> str:
        return self.nodes.get(node_id, node_id)


class Monitor(serialDevice):
    def __init__(self, port: str, baudrate: int, rx_queue: queue.Queue) -> None:
        super().__init__(port, baudrate)
        self.rx_queue = rx_queue
        self.running = True

    def start(self) -> None:
        self.open()
        threading.Thread(target=self._recv_worker, daemon=True).start()

    def _recv_worker(self) -> None:
        while self.running:
            try:
                data = self.recv()
                if data:
                    self.rx_queue.put(data)
            except Exception:
                break

    def stop(self) -> None:
        self.running = False
        self.close()


class ChatApp(App):
    CSS = "Screen { layout: vertical; }"
    BINDINGS = [Binding("q", "quit", "Quit")]

    def __init__(self, monitor: Monitor) -> None:
        super().__init__()
        self.monitor = monitor
        self.rx_queue = monitor.rx_queue
        self.keys = [base64.b64decode(k) for k in DEFAULT_KEYS]
        self.node_registry = NodeRegistry()
        self.table = DataTable()
        self.status = Static()

    def compose(self) -> ComposeResult:
        yield Container(self.table)
        yield Footer()
        yield self.status

    async def on_mount(self) -> None:
        self.table.add_columns("Time", "From", "Message")
        self.set_interval(0.1, self._pump_queue)

    def _pump_queue(self) -> None:
        while not self.rx_queue.empty():
            frame = self.rx_queue.get_nowait()
            asyncio.create_task(self._process_frame(frame))

    async def _process_frame(self, frame: bytes) -> None:
        self.status.update(f"[dim]{frame.hex(' ')}[/dim]")
        try:
            raw = extract_frame(frame)
            fields = extract_fields(raw)
        except Exception as e:
            self.status.update(f"[red]Error: {e}[/red]")
            return

        sender_hex = fields["sender"].hex().upper()
        payload = fields["payload"]

        pb = mesh_pb2.Data()
        try:
            pb.ParseFromString(payload)
            if self._handle_data_packet(pb, sender_hex):
                self.status.update("[green]Mensaje sin encriptar[/green]")
                return
        except Exception:
            pass

        for key_b64 in DEFAULT_KEYS:
            key = base64.b64decode(key_b64)
            try:
                decrypted = decrypt(payload, key, fields["sender"], fields["packet_id"])
                pb = mesh_pb2.Data()
                pb.ParseFromString(decrypted)
                if self._handle_data_packet(pb, sender_hex):
                    self.status.update(f"[green]Desencriptado[/green]")
                    return
            except Exception:
                continue

        self.status.update(f"[yellow]No desencriptable con {len(DEFAULT_KEYS)} claves[/yellow]")

    def _handle_data_packet(self, pb: mesh_pb2.Data, sender_hex: str) -> bool:
        if pb.portnum == 4:
            try:
                user = mesh_pb2.User()
                user.ParseFromString(pb.payload)
                self.node_registry.set_from_user_pb(user)
            except Exception:
                pass
            return True

        if pb.portnum == 1:
            try:
                text = pb.payload.decode("utf-8", errors="replace")
            except Exception:
                text = "<error>"
            msg = ChatMessage(
                ts=time.time(),
                sender_name=self.node_registry.resolve(sender_hex),
                text=sanitize_text(text),
            )
            self.table.add_row(*msg.as_row())
            return True
        return False

    def action_quit(self) -> None:
        self.exit()


async def run_app(args):
    rx_queue = queue.Queue()
    mon = Monitor(args.port, args.baudrate, rx_queue)
    mon.start()
    try:
        await ChatApp(mon).run_async()
    finally:
        mon.stop()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("-p", "--port", default=find_serial_port())
    parser.add_argument("-baud", "--baudrate", type=int, default=DEFAULT_BAUDRATE)
    args = parser.parse_args()
    asyncio.run(run_app(args))


if __name__ == "__main__":
    main()
