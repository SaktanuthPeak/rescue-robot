---
name: embedded-linux-rpi-deployment
description: Headless Raspberry Pi 4/5 deployment, offline WiFi Access Point (AP Mode) configuration, Linux serial port & USB permissions (udev/dialout), and systemd service management for autonomous robot startup.
---

# Raspberry Pi Linux & Edge Robot Deployment Skill

This skill provides step-by-step procedures, shell scripts, and systemd service definitions for deploying the full software stack (FastAPI backend + Svelte frontend) on **Raspberry Pi 4/5** running Raspberry Pi OS (Debian Linux).

---

## 1. Linux Serial Port Permissions & USB Udev Rules

Ensure non-root services can communicate with the ESP32 via USB Serial:

```bash
# 1. Add current user to dialout and tty groups
sudo usermod -a -G dialout,tty $USER

# 2. Create udev rule for persistent USB serial naming and full permissions
sudo tee /etc/udev/rules.d/99-robot-esp32.rules << 'RULES'
# ESP32 CH340 / CP2102 / FTDI
SUBSYSTEM=="tty", ATTRS{idVendor}=="10c4", ATTRS{idProduct}=="ea60", MODE="0666", SYMLINK+="robot_esp32"
SUBSYSTEM=="tty", ATTRS{idVendor}=="1a86", ATTRS{idProduct}=="7523", MODE="0666", SYMLINK+="robot_esp32"
SUBSYSTEM=="tty", KERNEL=="ttyUSB*", MODE="0666", GROUP="dialout"
SUBSYSTEM=="tty", KERNEL=="ttyACM*", MODE="0666", GROUP="dialout"
RULES

sudo udevadm control --reload-rules && sudo udevadm trigger
```

---

## 2. Offline WiFi Access Point (Hotspot) Setup

Enable the robot to broadcast its own WiFi network so students can connect their tablets or laptops directly without an external router:

### Using NetworkManager (`nmcli` - Standard on modern Raspberry Pi OS Bookworm):

```bash
# Create a standalone WiFi Hotspot
sudo nmcli con add type wifi ifname wlan0 con-name "Robot-AP" autoconnect yes ssid "RescueRobot-WiFi"
sudo nmcli con modify "Robot-AP" 802-11-wireless.mode ap 802-11-wireless.band bg
sudo nmcli con modify "Robot-AP" 802-11-wireless-security.key-mgmt wpa-psk
sudo nmcli con modify "Robot-AP" 802-11-wireless-security.psk "robot1234"
sudo nmcli con modify "Robot-AP" ipv4.method shared ipv4.addresses 192.168.4.1/24

# Start the Access Point
sudo nmcli con up "Robot-AP"
```

---

## 3. Systemd Services (Auto-Start on Boot)

### Service 1: Backend API Service (`/etc/systemd/system/rescue-robot-backend.service`)

```ini
[Unit]
Description=Rescue Robot FastAPI & Telemetry Service
After=network.target network-online.target

[Service]
Type=simple
User=pi
WorkingDirectory=/home/pi/rescue-robot/backend
ExecStart=/home/pi/rescue-robot/backend/.venv/bin/python -m uvicorn apiapp.main:app --host 0.0.0.0 --port 9000
Restart=always
RestartSec=3
Environment="PYTHONUNBUFFERED=1"

[Install]
WantedBy=multi-user.target
```

### Service 2: Frontend UI Static Host / Node Server (`/etc/systemd/system/rescue-robot-frontend.service`)

```ini
[Unit]
Description=Rescue Robot Svelte Web Dashboard
After=network.target rescue-robot-backend.service

[Service]
Type=simple
User=pi
WorkingDirectory=/home/pi/rescue-robot/frontend
ExecStart=/usr/bin/node build
Restart=always
RestartSec=3
Environment="PORT=3000"
Environment="HOST=0.0.0.0"

[Install]
WantedBy=multi-user.target
```

### Enabling and Starting Services:

```bash
sudo systemctl daemon-reload
sudo systemctl enable rescue-robot-backend.service
sudo systemctl enable rescue-robot-frontend.service
sudo systemctl start rescue-robot-backend.service
sudo systemctl start rescue-robot-frontend.service
```

---

## 4. Resource & Thermal Optimization on Raspberry Pi

- Keep logging minimal in production to prevent SD card wear (use logrotate or volatile journald).
- Configure power governor:
  ```bash
  echo "performance" | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
  ```
