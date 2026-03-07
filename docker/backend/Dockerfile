FROM python:3.13-alpine AS builder
COPY --from=ghcr.io/astral-sh/uv:latest /uv /uvx /bin/
WORKDIR /app
COPY backend/pyproject.toml backend/uv.lock ./

RUN apk add --no-cache gcc g++ python3-dev krb5-dev musl-dev
RUN /bin/uv sync --frozen --no-dev --no-cache

FROM python:3.13-alpine AS runner

COPY --from=ghcr.io/astral-sh/uv:latest /uv /bin/uv
WORKDIR /app

RUN apk add --no-cache krb5-libs

COPY --from=builder /app/.venv /app/.venv
COPY backend/src/ ./

ENV PATH="/app/.venv/bin:$PATH"

EXPOSE 8000
CMD ["python", "main.py"]