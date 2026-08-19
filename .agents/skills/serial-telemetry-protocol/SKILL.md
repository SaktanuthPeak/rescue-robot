---
name: serial-telemetry-protocol
description: Bidirectional serial communication protocol and telemetry streaming between Raspberry Pi (Python FastAPI / PySerial) and ESP32 microcontroller, packet framing, JSON/Binary parser, error recovery, and WebSocket broadcasting.
---

# Serial & Telemetry Communication Protocol Skill

This skill defines the communication standards, protocol framing, and Python/C++ code patterns for reliable bidirectional data exchange between the Raspberry Pi (High-Level Server) and the ESP32 (Low-Level Motion Controller).

---

## 1. Protocol Design Philosophy

The system uses a **Line-Based Text Protocol (ASCII)** with optional checksum for human readability & debugging, with support for fast binary packet telemetry streaming.

### Command Format (Downlink: Raspberry Pi -> ESP32)
Format: `CMD:<ARG1>,<ARG2>,...[\n]`

Examples:
- `MOVE:30,80\n` -> Move forward 30 cm at 80% speed (Negative distance = backward)
- `TURN:90,60\n` -> Turn right 90 degrees at 60% speed (Negative = left)
- `DRIVE:150,-150\n` -> Raw differential drive speed (Left PWM, Right PWM) for Virtual Joystick
- `STOP\n` -> Immediate emergency stop
- `NEO:255,0,0\n` -> Set NeoPixel color to Red (RGB 0-255)
- `TONE:440,200\n` -> Play frequency 440 Hz (Note A4) for 200 ms
- `SERVO:90\n` -> Set LiDAR servo angle (0-180 deg)
- `PING\n` -> Heartbeat keepalive / watchdog feed

### Telemetry Packet (Uplink: ESP32 -> Raspberry Pi)
Streamed at 20-50 Hz as JSON or compact comma-separated format:

```json
{"t":"TLM","d_l":1240,"d_r":1242,"dist":45,"bat":7.84,"ir":[0,0,1,0,0,0],"state":"IDLE"}
```
- `t`: Message type (`TLM` for telemetry, `ACK` for command acknowledgement, `ERR` for error)
- `d_l`, `d_r`: Accumulated left & right encoder ticks
- `dist`: Front TF-Luna distance in cm
- `bat`: Battery voltage in Volts (measured via resistor divider)
- `ir`: Array of IR obstacle states (`1` = obstacle detected, `0` = clear)
- `state`: Robot state (`IDLE`, `MOVING`, `BLOCKED`, `ESTOP`)

---

## 2. ESP32 Command Parser Implementation (C++)

```cpp
#include <Arduino.h>

void parse_serial_command(String line) {
    line.trim();
    if (line.length() == 0) return;

    if (line.startsWith("MOVE:")) {
        int commaIndex = line.indexOf(',');
        float dist_cm = 0;
        int speed = 70;
        if (commaIndex > 0) {
            dist_cm = line.substring(5, commaIndex).toFloat();
            speed = line.substring(commaIndex + 1).toInt();
        } else {
            dist_cm = line.substring(5).toFloat();
        }
        execute_move(dist_cm, speed);
        Serial.println("{\"t\":\"ACK\",\"cmd\":\"MOVE\",\"ok\":true}");
    }
    else if (line.startsWith("TURN:")) {
        int commaIndex = line.indexOf(',');
        float deg = 0;
        int speed = 60;
        if (commaIndex > 0) {
            deg = line.substring(5, commaIndex).toFloat();
            speed = line.substring(commaIndex + 1).toInt();
        } else {
            deg = line.substring(5).toFloat();
        }
        execute_turn(deg, speed);
        Serial.println("{\"t\":\"ACK\",\"cmd\":\"TURN\",\"ok\":true}");
    }
    else if (line.startsWith("DRIVE:")) {
        int commaIndex = line.indexOf(',');
        if (commaIndex > 0) {
            int left_pwm = line.substring(6, commaIndex).toInt();
            int right_pwm = line.substring(commaIndex + 1).toInt();
            set_raw_motors(left_pwm, right_pwm);
            feed_watchdog();
        }
    }
    else if (line == "STOP") {
        emergency_stop();
        Serial.println("{\"t\":\"ACK\",\"cmd\":\"STOP\",\"ok\":true}");
    }
    else if (line == "PING") {
        feed_watchdog();
        Serial.println("{\"t\":\"PONG\"}");
    }
}
```

---

## 3. Python FastAPI Serial Service (PySerial Async Worker)

```python
import asyncio
import json
import logging
import serial
import serial.tools.list_ports
from typing import Optional, Set
from fastapi import WebSocket

logger = logging.getLogger("serial_service")

class RobotSerialManager:
    def __init__(self, baudrate: int = 115200):
        self.baudrate = baudrate
        self.serial_conn: Optional[serial.Serial] = None
        self.is_running = False
        self.subscribers: Set[WebSocket] = set()
        self.latest_telemetry: dict = {}

    def auto_detect_port(self) -> Optional[str]:
        ports = serial.tools.list_ports.comports()
        for port in ports:
            # Look for typical ESP32 USB-UART chips (CP210x, CH340, FTDI)
            if any(vid in port.description.lower() or vid in port.hwid.lower() 
                   for vid in ["cp210", "ch340", "ftdi", "usb serial", "uart"]):
                return port.device
        # Default fallback on Raspberry Pi Linux
        for fallback in ["/dev/ttyUSB0", "/dev/ttyACM0", "/dev/ttyUSB1"]:
            if os.path.exists(fallback):
                return fallback
        return None

    async def connect(self):
        port = self.auto_detect_port()
        if not port:
            logger.warning("No ESP32 serial port detected, retrying...")
            return False
        try:
            self.serial_conn = serial.Serial(port, self.baudrate, timeout=0.1)
            logger.info(f"Connected to ESP32 on {port} @ {self.baudrate} baud")
            return True
        except Exception as e:
            logger.error(f"Failed to connect to {port}: {e}")
            return False

    async def run_loop(self):
        self.is_running = True
        buffer = bytearray()
        while self.is_running:
            if not self.serial_conn or not self.serial_conn.is_open:
                connected = await self.connect()
                if not connected:
                    await asyncio.sleep(2.0)
                    continue

            try:
                # Read incoming serial non-blockingly
                if self.serial_conn.in_waiting > 0:
                    data = self.serial_conn.read(self.serial_conn.in_waiting)
                    buffer.extend(data)
                    
                    while b'\n' in buffer:
                        line_bytes, buffer = buffer.split(b'\n', 1)
                        line = line_bytes.decode('utf-8', errors='ignore').strip()
                        if line.startswith("{") and line.endswith("}"):
                            try:
                                payload = json.loads(line)
                                self.latest_telemetry = payload
                                await self.broadcast(payload)
                            except json.JSONDecodeError:
                                pass
                else:
                    await asyncio.sleep(0.01)
            except Exception as e:
                logger.error(f"Serial read error: {e}")
                if self.serial_conn:
                    self.serial_conn.close()
                self.serial_conn = None
                await asyncio.sleep(1.0)

    async def send_command(self, cmd_string: str):
        if self.serial_conn and self.serial_conn.is_open:
            cmd = (cmd_string.strip() + "\n").encode('utf-8')
            self.serial_conn.write(cmd)
            self.serial_conn.flush()

    async def broadcast(self, data: dict):
        dead_sockets = set()
        for ws in list(self.subscribers):
            try:
                await ws.send_json(data)
            except Exception:
                dead_sockets.add(ws)
        self.subscribers.difference_update(dead_sockets)
```
