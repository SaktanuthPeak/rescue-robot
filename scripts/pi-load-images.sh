#!/usr/bin/env bash
# Run this ON THE PI after firebot-images.tar.gz has been scp'd over (deploy-to-pi.sh
# does this automatically over SSH -- this script is the manual fallback for when that
# SSH step drops, or for re-running the load without re-running the whole pipeline).
#
# Usage (on the Pi):
#   ./scripts/pi-load-images.sh
#   ./scripts/pi-load-images.sh ~/some-other-archive.tar.gz
#   EXTRA_COMPOSE_FILES="" ./scripts/pi-load-images.sh   # skip the camera overlay
set -euo pipefail

ARCHIVE="${1:-$HOME/firebot-images.tar.gz}"
# USB webcam is attached by default now -- include docker-compose.camera.yml so
# /dev/video0 actually gets passed into the container. Override to "" (or add
# "-f docker-compose.serial.yml") if that changes.
EXTRA_COMPOSE_FILES="${EXTRA_COMPOSE_FILES:--f docker-compose.camera.yml}"

if [ ! -f "$ARCHIVE" ]; then
  echo "Archive not found: $ARCHIVE" >&2
  echo "Run scripts/deploy-to-pi.sh from the laptop first (or scp the archive here)." >&2
  exit 1
fi

echo "==> Loading images from $ARCHIVE"
gunzip -c "$ARCHIVE" | docker load
rm -f "$ARCHIVE"

echo "==> Images now on this Pi:"
docker image ls

echo "==> Restarting the stack with the freshly loaded images"
# pull_policy: build on the base compose file forces a rebuild on every 'up' otherwise
# -- see docker-compose.prebuilt.yml and docs/docker-deployment.md. --wait blocks until
# both services report healthy (or fails loudly) instead of silently leaving a container
# stuck in "Created" -- that happened once when an earlier 'up' got interrupted.
docker compose -f docker-compose.yml -f docker-compose.prebuilt.yml $EXTRA_COMPOSE_FILES up -d --wait backend frontend

echo "==> Done:"
docker compose ps
