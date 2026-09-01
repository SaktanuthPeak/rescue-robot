#!/usr/bin/env bash
# Run this ON THE PI after firebot-images.tar.gz has been scp'd over (deploy-to-pi.sh
# does this automatically over SSH -- this script is the manual fallback for when that
# SSH step drops, or for re-running the load without re-running the whole pipeline).
#
# Usage (on the Pi):
#   ./scripts/pi-load-images.sh
#   ./scripts/pi-load-images.sh ~/some-other-archive.tar.gz
set -euo pipefail

ARCHIVE="${1:-$HOME/firebot-images.tar.gz}"

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
# -- see docker-compose.prebuilt.yml and docs/docker-deployment.md.
docker compose -f docker-compose.yml -f docker-compose.prebuilt.yml up -d

echo "==> Done. docker compose ps to confirm both services are healthy."
