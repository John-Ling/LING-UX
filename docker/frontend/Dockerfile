FROM node:24-alpine AS base

RUN apk add --no-cache libc6-compat
ENV DEBUG="corepack"
RUN corepack enable pnpm
RUN corepack use pnpm@9.0

FROM base AS deps
WORKDIR /app

COPY frontend/package.json frontend/pnpm-lock.yaml ./
RUN pnpm install --frozen-lockfile

FROM base AS builder
ARG VITE_API_BASE_URL
ENV NODE_ENV=production
ENV VITE_API_BASE_URL=$VITE_API_BASE_URL
WORKDIR /app
COPY --from=deps /app/node_modules ./node_modules
COPY frontend .

RUN pnpm run build

FROM nginx:alpine AS runner
COPY --from=builder /app/dist /usr/share/nginx/html
COPY docker/frontend/nginx.conf /etc/nginx/conf.d/default.conf
EXPOSE 80
CMD ["nginx", "-g", "daemon off;"]
