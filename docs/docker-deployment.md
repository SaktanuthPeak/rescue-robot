# Docker deployment (Raspberry Pi)

Two containers, one network:

| Service    | Image base        | Role                                                             |
| ---------- | ----------------- | ---------------------------------------------------------------- |
| `backend`  | `python:3.12-slim`| FastAPI + telemetry hub on port 9000                              |
| `frontend` | `nginx:1.27-alpine`| Serves the prerendered SvelteKit bundle **and** proxies the API  |

The browser only ever talks to the frontend container. nginx forwards `/v1`, `/docs`,
`/redoc` and `/openapi.json` to the backend on the same origin, which keeps CORS out of
the picture and means the tablet needs exactly one address to remember.

## First run

```bash
cp .env.example .env
# set SECRET_KEY, and PUBLIC_API_URL to the address the browser will use
docker compose up -d --build
```

Dashboard: `http://<pi-address>/` — API docs: `http://<pi-address>/docs`.

## With the Arduino attached

Serial mode needs the USB device passed into the container, which is a separate overlay
because compose refuses to start when a `devices:` path is missing (that would break
every bench run without hardware):

```bash
docker compose -f docker-compose.yml -f docker-compose.serial.yml up -d --build
```

If `/dev/ttyACM0` is not the right port, set `FLAME_SERIAL_PORT` in `.env`. If the host's
`dialout` group is not gid 20, set `SERIAL_GROUP_ID` to `getent group dialout | cut -d: -f3`.
The udev rules in the `embedded-linux-rpi-deployment` skill still apply on the host — the
container inherits whatever permissions the device node has.

## PUBLIC_API_URL is a build-time value

Vite inlines `$env/static/public` at build time, and `resolveWsUrl()` feeds it to
`new URL()` to derive the `ws://` telemetry endpoint — so it must be an **absolute** URL,
and changing it requires `docker compose up -d --build`, not a restart.

Set it to the dashboard's own address (nginx proxies from there), not port 9000:

- Robot access point: `http://192.168.4.1`
- mDNS: `http://raspberrypi.local`
- Non-default `WEB_PORT`: include it, e.g. `http://192.168.4.1:8080`

## Operating

```bash
docker compose logs -f backend        # telemetry hub / serial reconnects
docker compose ps                     # both services report healthchecks
docker compose restart backend        # e.g. after replugging the Arduino
docker compose down                   # stop everything
```

`restart: unless-stopped` plus Docker's own systemd unit covers auto-start on boot, so
the per-service units in the `embedded-linux-rpi-deployment` skill are not needed when
running under compose — use one approach or the other, not both.

## Frontend build OOM on Pi 3 (1GB RAM)

`pnpm run build` runs a SvelteKit SSR+client `vite build`, which needs more heap than a
Pi 3's default swap (`dphys-swapfile`'s 100MB) leaves available — the build dies with
`FATAL ERROR: Ineffective mark-compacts near heap limit`. The Dockerfile now sets
`NODE_OPTIONS=--max-old-space-size=896` in the builder stage, but that ceiling only
helps once the system actually has that much memory to back it. Bump swap first:

```bash
sudo dphys-swapfile swapoff
sudo sed -i 's/^CONF_SWAPSIZE=.*/CONF_SWAPSIZE=2048/' /etc/dphys-swapfile
sudo dphys-swapfile setup
sudo dphys-swapfile swapon
free -h   # confirm ~2GB swap before retrying
docker compose up -d --build
```

The build gets slow (swap, not RAM) but should complete instead of crashing. Building
on the Pi is by design (see the `docker-compose.yml` header comment) — if a build is
still too slow to be practical, cross-build on a faster machine instead (below).

## Cross-building on a laptop, running on the Pi

Skips the Pi's build step entirely: build both images on a machine with real RAM/CPU,
then ship the images over instead of the source.

`scripts/deploy-to-pi.sh` automates steps 2-5 below:

```bash
PI_HOST=saktanuth@192.168.1.42 ./scripts/deploy-to-pi.sh
```

Override `PLATFORM` (default `linux/arm64`; use `linux/arm/v7` for 32-bit Raspberry Pi
OS) and `PUBLIC_API_URL` (default `http://192.168.4.1`) as env vars if they differ from
the defaults. Manual steps, for reference or troubleshooting:

1. **Check the Pi's CPU architecture** (32-bit and 64-bit Raspberry Pi OS need
   different `--platform`):

   ```bash
   ssh <user>@<pi-address> uname -m
   # armv7l -> linux/arm/v7      aarch64 -> linux/arm64
   ```

2. **One-time on the laptop** — register QEMU so buildx can emulate that architecture
   (skip if Docker Desktop, which bundles this already):

   ```bash
   docker run --privileged --rm tonistiigi/binfmt --install all
   ```

3. **Build both images for the Pi's architecture**, tagged to match the names
   `docker compose` would generate on the Pi (`<project-dir>-<service>` —
   `rescue-robot-backend` / `rescue-robot-frontend` for this repo). Pass the same
   `PUBLIC_API_URL` the Pi's `.env` uses:

   ```bash
   PLATFORM=linux/arm64   # or linux/arm/v7 — from step 1

   docker buildx build --platform $PLATFORM --target runtime \
     -t rescue-robot-backend:latest --load ./backend

   docker buildx build --platform $PLATFORM --target runtime \
     --build-arg PUBLIC_API_URL=http://192.168.4.1 \
     -t rescue-robot-frontend:latest --load ./frontend
   ```

4. **Ship and load the images:**

   ```bash
   docker save rescue-robot-backend:latest rescue-robot-frontend:latest \
     | gzip > firebot-images.tar.gz
   scp firebot-images.tar.gz <user>@<pi-address>:~/
   ssh <user>@<pi-address> 'gunzip -c firebot-images.tar.gz | docker load'
   ```

5. **On the Pi, start with the `prebuilt` overlay**, which is required, not optional:
   the base `docker-compose.yml` sets `pull_policy: build` (see its header comment) so
   that a bare `docker compose up` never tries a registry pull -- but that setting also
   makes Compose rebuild from source on *every* `up`, even when a matching image is
   already loaded. `docker-compose.prebuilt.yml` overrides `pull_policy` back to
   `never` for both services so the loaded images actually get used:

   ```bash
   docker compose -f docker-compose.yml -f docker-compose.prebuilt.yml up -d
   ```

Re-run steps 3–5 whenever source changes; `docker compose up -d --build` on the Pi
would silently rebuild from scratch and undo the point of this, so avoid `--build`
there unless you're deliberately going back to on-device builds.

## Regenerating the API client

`API_PORT` (default 9000) is published so the backend stays reachable directly:

```bash
cd frontend && pnpm run openapi
```
