FROM python:3.13-slim AS builder
COPY --from=ghcr.io/astral-sh/uv:latest /uv /uvx /bin/
WORKDIR /app
COPY backend/pyproject.toml backend/uv.lock ./

RUN apt-get update && apt-get install -y \
    gcc g++ python3-dev libkrb5-dev \
    && rm -rf /var/lib/apt/lists/*

RUN /bin/uv sync --frozen --no-dev

FROM python:3.13-slim AS runner
COPY --from=ghcr.io/astral-sh/uv:latest /uv /bin/uv
WORKDIR /app

RUN apt-get update && apt-get install -y libkrb5-3 && rm -rf /var/lib/apt/lists/*

COPY backend/src/ ./

EXPOSE 8000
CMD ["uv", "run", "src/main.py"]