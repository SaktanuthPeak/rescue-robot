#!/usr/bin/env bash
# Configure Raspberry Pi Wi-Fi AP mode and start the FireBot Docker stack.
# Raspberry Pi OS Bookworm / NetworkManager is required.
#
# Usage:
#   sudo ./scripts/setup-rpi-ap.sh
#   sudo ./scripts/setup-rpi-ap.sh --serial --camera
#   sudo RPI_SSID=MyRobot RPI_PASSWORD='strong-password' ./scripts/setup-rpi-ap.sh
#   sudo ./scripts/setup-rpi-ap.sh --no-compose

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

CON_NAME="${RPI_CONNECTION_NAME:-Robot-AP}"
SSID="${RPI_SSID:-RescueRobot-WiFi}"
PASSWORD="${RPI_PASSWORD:-robot1234}"
IFACE="${RPI_WIFI_IFACE:-wlan0}"
AP_IP="${RPI_AP_IP:-192.168.4.1}"
WEB_PORT="${RPI_WEB_PORT:-80}"
API_PORT="${RPI_API_PORT:-9000}"
PUBLIC_API_URL="${RPI_PUBLIC_API_URL:-http://${AP_IP}}"

WITH_SERIAL=0
WITH_CAMERA=0
RUN_COMPOSE=1
BUILD_IMAGES=1

usage() {
  sed -n '1,16p' "$0"
  echo
  echo "Options: --serial --camera --no-compose --no-build"
}

for arg in "$@"; do
  case "$arg" in
    --serial) WITH_SERIAL=1 ;;
    --camera) WITH_CAMERA=1 ;;
    --no-compose) RUN_COMPOSE=0 ;;
    --no-build) BUILD_IMAGES=0 ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown option: $arg" >&2; usage >&2; exit 2 ;;
  esac
done

if [[ "$EUID" -ne 0 ]]; then
  echo "Run this script with sudo." >&2
  exit 1
fi

for command_name in nmcli ip; do
  if ! command -v "$command_name" >/dev/null 2>&1; then
    echo "Missing command: $command_name. Install NetworkManager first." >&2
    exit 1
  fi
done

if ! nmcli device show "$IFACE" >/dev/null 2>&1; then
  echo "Wi-Fi interface not found: $IFACE" >&2
  echo "Check with: nmcli device status" >&2
  exit 1
fi

if (( ${#PASSWORD} < 8 )); then
  echo "RPI_PASSWORD must contain at least 8 characters." >&2
  exit 1
fi

echo "==> Configuring Wi-Fi access point '$SSID' on $IFACE"
nmcli radio wifi on

if nmcli -t -f NAME connection show | grep -Fxq "$CON_NAME"; then
  nmcli connection modify "$CON_NAME" \
    connection.interface-name "$IFACE" \
    connection.autoconnect yes \
    connection.autoconnect-priority 100 \
    802-11-wireless.ssid "$SSID" \
    802-11-wireless.mode ap \
    802-11-wireless.band bg \
    802-11-wireless-security.key-mgmt wpa-psk \
    802-11-wireless-security.psk "$PASSWORD" \
    ipv4.method shared \
    ipv4.addresses "$AP_IP/24" \
    ipv6.method disabled
else
  nmcli connection add type wifi ifname "$IFACE" con-name "$CON_NAME" \
    autoconnect yes ssid "$SSID"
  nmcli connection modify "$CON_NAME" \
    connection.autoconnect-priority 100 \
    802-11-wireless.mode ap \
    802-11-wireless.band bg \
    802-11-wireless-security.key-mgmt wpa-psk \
    802-11-wireless-security.psk "$PASSWORD" \
    ipv4.method shared \
    ipv4.addresses "$AP_IP/24" \
    ipv6.method disabled
fi

nmcli connection up "$CON_NAME"
echo "==> Access point is ready"
ip -4 addr show "$IFACE" | sed -n '1,8p'

if (( RUN_COMPOSE )); then
  command -v docker >/dev/null 2>&1 || {
    echo "Docker is not installed or not in PATH." >&2
    exit 1
  }

  compose_args=(
    -f docker-compose.yml
    -f docker-compose.rpi-ap.yml
  )
  (( WITH_SERIAL )) && compose_args+=( -f docker-compose.serial.yml )
  (( WITH_CAMERA )) && compose_args+=( -f docker-compose.camera.yml )

  echo "==> Starting FireBot containers"
  cd "$PROJECT_DIR"
  if (( BUILD_IMAGES )); then
    RPI_PUBLIC_API_URL="$PUBLIC_API_URL" \
    RPI_WEB_PORT="$WEB_PORT" \
    RPI_API_PORT="$API_PORT" \
      docker compose "${compose_args[@]}" up -d --build
  else
    RPI_PUBLIC_API_URL="$PUBLIC_API_URL" \
    RPI_WEB_PORT="$WEB_PORT" \
    RPI_API_PORT="$API_PORT" \
      docker compose "${compose_args[@]}" up -d
  fi

  docker compose "${compose_args[@]}" ps
fi

echo
echo "Connect to Wi-Fi: $SSID"
echo "Dashboard: ${PUBLIC_API_URL}/"
echo "API docs:  ${PUBLIC_API_URL}:${API_PORT}/docs"
