# syntax=docker/dockerfile:1

# ---- builder: SvelteKit static (adapter-static) build ---------------------------------
FROM node:22-alpine AS builder

# PUBLIC_API_URL is inlined by Vite at build time ($env/static/public), so it is a build
# arg, not a runtime env var. It must be an absolute URL: resolveWsUrl() feeds it to
# `new URL()` to derive the ws:// telemetry endpoint.
ARG PUBLIC_API_URL=http://192.168.4.1
ARG PUBLIC_APP_TITLE=FireBot
ENV PUBLIC_API_URL=$PUBLIC_API_URL \
    PUBLIC_APP_TITLE=$PUBLIC_APP_TITLE

RUN corepack enable && corepack prepare pnpm@10.15.0 --activate

WORKDIR /app

COPY package.json pnpm-lock.yaml pnpm-workspace.yaml .npmrc ./
RUN pnpm install --frozen-lockfile

COPY . .
RUN pnpm run build

# ---- runtime: nginx serves the static bundle and proxies the API ----------------------
FROM nginx:1.27-alpine AS runtime

COPY nginx.conf /etc/nginx/conf.d/default.conf
COPY --from=builder /app/build /usr/share/nginx/html

EXPOSE 80

HEALTHCHECK --interval=15s --timeout=5s --start-period=10s --retries=3 \
    CMD wget -q -O /dev/null http://127.0.0.1/ || exit 1
