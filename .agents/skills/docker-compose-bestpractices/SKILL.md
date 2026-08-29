---
name: docker-compose-bestpractices
description: Comprehensive guide and best practices for Docker Compose v2 specification, multi-container orchestration, environment variables & secret management, healthchecks, networking, multi-stage builds, compose profiles, volume/resource limits, and hardware passthrough.
---

# Docker Compose Best Practices Skill

This skill provides comprehensive standards, architectural patterns, and production-tested recipes for building, orchestrating, and maintaining multi-container applications using **Docker Compose v2** (Compose Specification).

---

## 1. Core Compose v2 Specification & Syntax

### Deprecated vs. Modern Compose
- **No `version:` key**: The top-level `version: "3.x"` field is deprecated and obsolete in Compose v2. Omit it completely.
- **Use `docker compose`**: Always invoke the modern Compose v2 CLI plugin (`docker compose`), never the legacy Python-based v1 standalone binary (`docker-compose`).
- **Standard Root Keys**: Only use modern top-level keys: `services`, `networks`, `volumes`, `secrets`, `configs`.

### Recommended Service Definition Structure
Keep service keys in a consistent, logical hierarchy across all compose files:

```yaml
services:
  service-name:
    # 1. Image / Build definition
    build:
      context: ./backend
      dockerfile: Dockerfile
      target: runtime
    pull_policy: build   # Options: build, missing, always, never
    image: myorg/service-name:latest # Optional when publishing
    container_name: service-name

    # 2. Lifecycle & Runtime
    restart: unless-stopped
    init: true           # Runs a lightweight init process (tini) to reap zombies & forward signals
    user: "1000:1000"    # Enforce non-root execution

    # 3. Environment & Secrets
    env_file:
      - .env
    environment:
      APP_ENV: production
      DATABASE_URL: "${DATABASE_URL:?DATABASE_URL is required}"

    # 4. Networking & Ports
    ports:
      - "127.0.0.1:8080:80" # Bind to localhost unless public ingress is intended
    expose:
      - "9000"              # Document internal ports accessible to other services on the network
    networks:
      - internal-net

    # 5. Dependency Order & Health
    depends_on:
      database:
        condition: service_healthy
        restart: true

    healthcheck:
      test: ["CMD-SHELL", "curl -f http://127.0.0.1:9000/v1/health || exit 1"]
      interval: 15s
      timeout: 5s
      retries: 3
      start_period: 10s

    # 6. Storage & Mounts
    volumes:
      - app-data:/app/data
      - ./config.json:/app/config.json:ro # Read-only bind mount

    # 7. Production Hardening & Resources
    logging:
      driver: json-file
      options:
        max-size: "10m"
        max-file: "3"
    deploy:
      resources:
        limits:
          cpus: "1.0"
          memory: 512M
        reservations:
          memory: 128M
```

---

## 2. Environment Variables & Secret Handling

### Interpolation Operators
Docker Compose supports bash-style parameter expansion:
- `${VARIABLE:-default}`: Use `default` if `VARIABLE` is unset or empty.
- `${VARIABLE-default}`: Use `default` only if `VARIABLE` is unset.
- `${VARIABLE:?error message}`: Fail with error message if `VARIABLE` is unset or empty.
- `${VARIABLE?error message}`: Fail only if `VARIABLE` is unset.
- `$$VARIABLE`: Escape literal `$` signs (useful for Nginx or inline shell scripts).

### Environment Variable Precedence (Lowest to Highest)
1. Environment variables set inside the container image (`ENV` in Dockerfile).
2. Variables defined in `env_file:` within `docker-compose.yml`.
3. Variables defined in `environment:` within `docker-compose.yml`.
4. Variables in the `.env` file in the project root (interpolated into Compose).
5. Environment variables from the host shell executing `docker compose`.
6. CLI arguments passed with `--env` or `--env-file`.

### Secret Management Rules
- **Never commit `.env` containing production credentials** to version control.
- **Provide a `.env.example`** with safe defaults and clear variable descriptions.
- **Use Docker Secrets** for sensitive data (API keys, certificates, passwords):

```yaml
services:
  backend:
    secrets:
      - db_password
      - api_token

secrets:
  db_password:
    file: ./secrets/db_password.txt
  api_token:
    environment: "PROD_API_TOKEN" # Secret supplied via host environment variable
```

---

## 3. Healthchecks & Startup Dependency Orchestration

### The `depends_on` Fallacy
Simply listing `depends_on: [db]` only waits for the database container to *start*, not for the database engine to accept connections. Always use `condition: service_healthy` or `condition: service_completed_successfully`.

### Healthcheck Conditions

```yaml
services:
  migration:
    image: myorg/migration-runner
    depends_on:
      db:
        condition: service_healthy

  backend:
    build: ./backend
    depends_on:
      db:
        condition: service_healthy
      migration:
        condition: service_completed_successfully # Waits for one-shot task to finish with code 0

  frontend:
    build: ./frontend
    depends_on:
      backend:
        condition: service_healthy
```

### Writing Lightweight Healthchecks
Avoid bloated tools in production containers. Use built-in utilities or runtime one-liners:

```yaml
# HTTP Web Service (curl)
healthcheck:
  test: ["CMD", "curl", "-f", "http://localhost:8080/health"]
  interval: 10s
  timeout: 3s
  retries: 3
  start_period: 5s

# Alpine / Nginx (wget)
healthcheck:
  test: ["CMD-SHELL", "wget -q -O /dev/null http://127.0.0.1/ || exit 1"]

# Python Application (zero external tool dependency)
healthcheck:
  test: ["CMD", "python", "-c", "import urllib.request, sys; sys.exit(0 if urllib.request.urlopen('http://127.0.0.1:9000/v1/health').status == 200 else 1)"]

# PostgreSQL Database
healthcheck:
  test: ["CMD-SHELL", "pg_isready -U ${POSTGRES_USER:-postgres} -d ${POSTGRES_DB:-app}"]

# Redis Database
healthcheck:
  test: ["CMD", "redis-cli", "ping"]
```

---

## 4. Multi-Environment Stacks & Compose Overrides

### The Compose File Overlay Pattern
Avoid duplicating entire compose files across environments. Structure configurations hierarchically:

1. **`docker-compose.yml`**: Base definition containing shared services, networks, volumes, and healthchecks.
2. **`docker-compose.override.yml`**: Automatically applied by `docker compose up` for local development (bind mounts, debug ports, live reload).
3. **`docker-compose.prod.yml`**: Production overrides (resource limits, logging drivers, restart policies, reverse proxy ingress).
4. **`docker-compose.serial.yml` / `docker-compose.hardware.yml`**: Hardware/device overlays (isolated so simulation/mock stacks run without physical devices attached).

### Merging Behavior
- **Scalars & Strings** (e.g., `container_name`, `image`, `restart`): Overwritten by the last specified file.
- **Dictionaries** (e.g., `environment`, `labels`, `logging.options`): Merged by key (later files overwrite conflicting keys).
- **Arrays & Lists** (e.g., `ports`, `volumes`, `devices`, `expose`): Appended together (ensure no port conflict between base and override).

### Running Stacks with Overlays

```bash
# Local development (automatically includes docker-compose.yml + docker-compose.override.yml)
docker compose up -d

# Explicit Production Deployment
docker compose -f docker-compose.yml -f docker-compose.prod.yml up -d --build

# Edge Robot Stack with Hardware Passthrough
docker compose -f docker-compose.yml -f docker-compose.serial.yml up -d --build
```

---

## 5. Networking & Service Discovery

### Network Segmentation & Isolation
Group services into separate bridge networks based on access tiers:

```yaml
services:
  frontend:
    build: ./frontend
    networks:
      - public-net

  backend:
    build: ./backend
    networks:
      - public-net
      - internal-net

  database:
    image: postgres:16-alpine
    networks:
      - internal-net # Completely hidden from frontend and public ingress

networks:
  public-net:
    driver: bridge
  internal-net:
    driver: bridge
    internal: true   # Blocks external internet egress and non-member access
```

### DNS & Inter-Container Communication
- Containers on the same user-defined bridge network resolve each other by **service name** (e.g., `http://backend:9000`).
- **Never use `localhost` or `127.0.0.1`** to communicate between separate containers (each container has its own `localhost` loopback interface).
- Use `expose:` instead of `ports:` for internal traffic:
  - `expose`: Makes ports accessible *only* to other containers on the same network.
  - `ports`: Binds and exposes the port on the host machine.

### Binding to Localhost for Ingress Security
Prevent accidental exposure to external network interfaces by binding published ports explicitly:

```yaml
ports:
  # SAFE: Accessible only on the host machine
  - "127.0.0.1:9000:9000"
  
  # PUBLIC: Accessible from any network adapter (LAN / Internet)
  - "80:80"
```

---

## 6. Volumes, Data Persistence & File Permissions

### Volume Types & When to Use Them
1. **Named Volumes (`volume-name:/path`)**: Best for databases, caches, and persistent state managed by Docker. Fast and isolated across OS platforms.
2. **Bind Mounts (`./host-dir:/path`)**: Best for local source code mounting during active development.
3. **tmpfs Mounts (`type: tmpfs`)**: High-performance, in-memory volatile storage for transient data and sensitive temp files.

```yaml
services:
  backend:
    volumes:
      # Named volume for SQLite / uploaded media
      - backend-data:/app/data
      # Read-only configuration bind mount
      - ./config.yaml:/app/config.yaml:ro
      # In-memory ephemeral temp directory
      - type: tmpfs
        target: /app/tmp
        tmpfs:
          size: 64M

volumes:
  backend-data:
    driver: local
```

### Non-Root UID/GID Handling
To avoid permission clashes when files created inside containers are edited on the host:
- Set matching UID/GID via `user: "${UID:-1000}:${GID:-1000}"`.
- Ensure application directories in Dockerfiles have appropriate permissions (`chown -R app:app /app`).

---

## 7. Production Hardening & Resource Management

### Mandatory Log Rotation
Docker's default `json-file` driver writes unlimited logs to `/var/lib/docker/containers/...`, eventually exhausting disk space. Always configure rotation:

```yaml
services:
  backend:
    logging:
      driver: json-file
      options:
        max-size: "10m" # Rotate when file reaches 10 MB
        max-file: "3"   # Retain at most 3 archive files (total max 30 MB)
```

Alternatively, set logging defaults globally in `/etc/docker/daemon.json`.

### Resource Constraints
Prevent runaway processes or memory leaks from freezing the host:

```yaml
services:
  backend:
    deploy:
      resources:
        limits:
          cpus: "0.80"      # Maximum 80% of 1 CPU core
          memory: 512M      # Hard OOM killer threshold
        reservations:
          cpus: "0.10"
          memory: 128M      # Guaranteed minimum memory
```

### Security & Privilege Dropping
Apply the principle of least privilege:

```yaml
services:
  backend:
    security_opt:
      - no-new-privileges:true
    cap_drop:
      - ALL
    cap_add:
      - NET_BIND_SERVICE # Only re-enable capabilities strictly needed
    read_only: true       # Makes container root filesystem read-only
    tmpfs:
      - /tmp:rw,noexec,nosuid,size=32m
```

---

## 8. Hardware Passthrough & Edge / Embedded Robot Patterns

When deploying on Raspberry Pi or edge nodes interacting with microcontrollers (ESP32 / Arduino / USB sensors):

### Conditional Hardware Overlays
Never put required physical hardware paths directly in the base `docker-compose.yml`, as Docker Compose will fail to launch if the device node does not exist. Keep hardware in an override:

```yaml
# docker-compose.serial.yml
services:
  backend:
    environment:
      TELEMETRY_SOURCE: serial
    devices:
      - "${FLAME_SERIAL_PORT:-/dev/ttyACM0}:${FLAME_SERIAL_PORT:-/dev/ttyACM0}"
    group_add:
      - "${SERIAL_GROUP_ID:-20}" # Host dialout GID (20 on Debian/Raspberry Pi OS)
```

### Granting Serial / USB Permissions
- Find host dialout GID: `getent group dialout | cut -d: -f3`
- Pass group ID to container via `group_add:` rather than running the container in insecure `--privileged` mode.
- If direct GPIO / SPI / I2C access is required, bind specific device nodes (`/dev/gpiomem`, `/dev/i2c-1`) and add necessary capabilities (`SYS_RAWIO`).

---

## 9. Compose Profiles, Build Optimization & Compose Watch

### Service Profiles
Use `profiles` to define optional, on-demand services (test runners, database GUIs, migration tools) without cluttering the standard `docker compose up` run:

```yaml
services:
  backend:
    build: ./backend

  db-admin:
    image: adminer:latest
    profiles: ["tools", "debug"]
    ports:
      - "127.0.0.1:8088:8080"

  e2e-tests:
    build: ./e2e
    profiles: ["test"]
```

```bash
# Start standard stack only
docker compose up -d

# Start standard stack plus database admin UI
docker compose --profile tools up -d

# Run tests profile only
docker compose run --rm e2e-tests
```

### Compose Watch (Live Development Reloading)
Modern Compose v2 features built-in file synchronization (`develop.watch`):

```yaml
services:
  frontend:
    build:
      context: ./frontend
      target: development
    develop:
      watch:
        - action: sync
          path: ./frontend/src
          target: /app/src
        - action: rebuild
          path: ./frontend/package.json
```

```bash
# Run with active sync
docker compose watch
```

---

## 10. CLI Reference & Verification Checklist

### Daily Operational Commands

| Command | Purpose |
| :--- | :--- |
| `docker compose config` | Validate, resolve variables, and render merged YAML output |
| `docker compose up -d --build` | Build images and launch services in background |
| `docker compose ps` | Check service status and healthcheck state |
| `docker compose logs -f --tail=100 <service>` | Tail real-time service logs |
| `docker compose exec <service> sh` | Open an interactive shell inside a running service |
| `docker compose top` | View active processes across all containers |
| `docker compose down --remove-orphans` | Gracefully stop containers and network |
| `docker compose down -v` | **Caution**: Stop containers and destroy named volumes |

### Pre-Deployment Best Practices Checklist
- [ ] No `version:` field present at the top of Compose files.
- [ ] No plaintext passwords or production secrets committed in repo.
- [ ] All inter-container dependencies use `condition: service_healthy`.
- [ ] Log rotation (`max-size` and `max-file`) configured on all services.
- [ ] Ingress ports bound to `127.0.0.1` unless intentional public access.
- [ ] Multi-stage Dockerfiles leveraged (`target: runtime`).
- [ ] Ephemeral or non-root `user:` defined.
- [ ] Physical device mounts isolated into separate override files.
- [ ] `docker compose config` passes without syntax errors or warnings.
