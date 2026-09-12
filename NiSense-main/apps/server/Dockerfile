# Multi-arch (arm64 for Raspberry Pi, amd64 for cloud) FastAPI image.
# Avoid apt-get so builds work on hosts with flaky Debian mirror access
# (psycopg2-binary ships its own libpq).
FROM python:3.11-slim AS base

ENV PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1 \
    PIP_NO_CACHE_DIR=1

WORKDIR /app

COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY . .

# Non-root runtime user.
RUN useradd -m -u 10001 nisense && mkdir -p /data/artifacts /data/keys \
    && chown -R nisense:nisense /app /data
USER nisense

EXPOSE 8000
HEALTHCHECK --interval=30s --timeout=5s --retries=5 \
    CMD python -c "import urllib.request; urllib.request.urlopen('http://127.0.0.1:8000/healthz')" || exit 1

# Prod default: single gunicorn worker on Pi-class hosts (override via compose).
CMD ["gunicorn", "app.main:app", "-k", "uvicorn.workers.UvicornWorker", \
     "-w", "1", "-b", "0.0.0.0:8000", "--timeout", "120"]
