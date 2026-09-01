#!/usr/bin/env bash
# Cross-build backend + frontend images for the Pi's arch, ship them over SSH, and
# start the stack there without a local build (avoids the Pi 3 OOM in vite build --
# see docs/docker-deployment.md "Cross-building on a laptop, running on the Pi").
#
# Usage:
#   ./scripts/deploy-to-pi.sh
#   PI_HOST=saktanuth@192.168.1.42 PLATFORM=linux/arm64 ./scripts/deploy-to-pi.sh
set -euo pipefail

PI_HOST="${PI_HOST:-saktanuth@192.168.1.42}"
PI_DIR="${PI_DIR:-~/rescue-robot}"
PLATFORM="${PLATFORM:-linux/arm64}"          # linux/arm/v7 for 32-bit Raspberry Pi OS
PUBLIC_API_URL="${PUBLIC_API_URL:-http://192.168.4.1}"
EXTRA_COMPOSE_FILES="${EXTRA_COMPOSE_FILES:-}" # e.g. "-f docker-compose.camera.yml -f docker-compose.serial.yml"

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

ARCHIVE="firebot-images.tar.gz"

echo "==> [1/5] Registering QEMU emulators (no-op if already installed)"
docker run --privileged --rm tonistiigi/binfmt --install all >/dev/null

echo "==> [2/5] Building backend image ($PLATFORM)"
docker buildx build --platform "$PLATFORM" --target runtime \
  -t rescue-robot-backend:latest --load ./backend

echo "==> [3/5] Building frontend bundle natively (PUBLIC_API_URL=$PUBLIC_API_URL)"
# Native build on the host, not under QEMU: SvelteKit's SSR+client `vite build` makes
# V8's JIT crash under arm64 emulation on an x86_64 host (qemu: uncaught target signal
# 6 / Aborted) no matter how much RAM the host has -- the emulation itself is the
# problem, not memory. Build the static bundle natively here, then just copy it into an
# arm64 nginx image below, which needs no computation and is unaffected by emulation.
(
  cd frontend
  corepack enable
  corepack prepare pnpm@10.15.0 --activate
  PUBLIC_API_URL="$PUBLIC_API_URL" PUBLIC_APP_TITLE="${PUBLIC_APP_TITLE:-FireBot}" \
    pnpm install --frozen-lockfile
  PUBLIC_API_URL="$PUBLIC_API_URL" PUBLIC_APP_TITLE="${PUBLIC_APP_TITLE:-FireBot}" \
    pnpm run build
)

echo "==> [3b/5] Packaging frontend image ($PLATFORM) -- static copy, no emulation needed"
# build/ is passed as the named context "frontend-build" instead of relying on the
# default ./frontend context, whose .dockerignore excludes build/ on purpose (that
# exclusion is correct for the main Dockerfile, which builds it fresh inside the image).
docker buildx build --platform "$PLATFORM" \
  -f frontend/Dockerfile.static \
  --build-context frontend-build=./frontend/build \
  -t rescue-robot-frontend:latest --load ./frontend

echo "==> [4/5] Saving + shipping images to $PI_HOST"
docker save rescue-robot-backend:latest rescue-robot-frontend:latest | gzip > "$ARCHIVE"
scp "$ARCHIVE" "$PI_HOST:~/"
rm -f "$ARCHIVE"

echo "==> [5/5] Loading images and starting the stack on $PI_HOST"
# shellcheck disable=SC2029
ssh "$PI_HOST" "
  set -euo pipefail
  gunzip -c ~/$ARCHIVE | docker load
  rm -f ~/$ARCHIVE
  cd $PI_DIR
  # docker-compose.yml sets pull_policy: build on purpose (never try a registry pull),
  # which also means a bare 'up' rebuilds from source every time. The prebuilt overlay
  # switches that to pull_policy: never so the just-loaded images get used as-is.
  docker compose -f docker-compose.yml -f docker-compose.prebuilt.yml $EXTRA_COMPOSE_FILES up -d
"

echo "==> Done. docker compose ps / logs on the Pi to confirm."
