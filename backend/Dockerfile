# syntax=docker/dockerfile:1

# ---- builder: resolve dependencies into a self-contained .venv ------------------------
FROM python:3.12-slim-bookworm AS builder

ENV PIP_DISABLE_PIP_VERSION_CHECK=1 \
    PIP_NO_CACHE_DIR=1 \
    POETRY_NO_INTERACTION=1

RUN pip install "poetry>=2.0,<3.0"

WORKDIR /app

# poetry.toml pins virtualenvs.in-project, so the venv lands at /app/.venv and can be
# copied wholesale into the runtime stage. The [k] glob makes poetry.lock optional so a
# checkout without it still builds (poetry resolves instead) rather than dying on COPY.
COPY pyproject.toml poetry.toml ./
COPY poetry.loc[k] ./
RUN poetry install --only main --no-root

# ---- runtime -------------------------------------------------------------------------
FROM python:3.12-slim-bookworm AS runtime

ENV PYTHONUNBUFFERED=1 \
    PYTHONDONTWRITEBYTECODE=1 \
    PATH="/app/.venv/bin:$PATH"

WORKDIR /app

COPY --from=builder /app/.venv /app/.venv
COPY apiapp ./apiapp
COPY cli ./cli
COPY pyproject.toml poetry.toml ./

# uid 1000 matches the default `pi` user, so a bind-mounted host directory keeps sane
# ownership. dialout (gid 20 on Debian) is what /dev/ttyACM* is owned by; the compose
# serial override still adds the *host* dialout gid, which can differ.
RUN groupadd --gid 1000 app \
    && useradd --uid 1000 --gid 1000 --create-home app \
    && usermod -aG dialout,video app
USER app

EXPOSE 9000

HEALTHCHECK --interval=15s --timeout=5s --start-period=20s --retries=3 \
    CMD python -c "import urllib.request,sys; sys.exit(0 if urllib.request.urlopen('http://127.0.0.1:9000/v1/health', timeout=4).status == 200 else 1)"

CMD ["fastapi", "run", "apiapp/main.py", "--host", "0.0.0.0", "--port", "9000"]
