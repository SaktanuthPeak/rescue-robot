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

## Regenerating the API client

`API_PORT` (default 9000) is published so the backend stays reachable directly:

```bash
cd frontend && pnpm run openapi
```
