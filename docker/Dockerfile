# ── Stage 1: Build Vue report template ──
FROM node:20-alpine AS report-build
WORKDIR /app
COPY report-app/package.json report-app/package-lock.json ./
RUN npm ci
COPY report-app/ .
RUN npm run build

# ── Stage 2: Digital twin runtime ──
FROM ubuntu:24.04

ARG RENODE_VERSION=1.15.3

ENV DEBIAN_FRONTEND=noninteractive
ENV PATH="/opt/renode:${PATH}"

# Install build dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    gcc-arm-none-eabi \
    libnewlib-arm-none-eabi \
    libstdc++-arm-none-eabi-newlib \
    cmake \
    build-essential \
    git \
    python3 \
    python3-matplotlib \
    wget \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Install Renode portable (includes its own Mono runtime)
RUN wget -q https://github.com/renode/renode/releases/download/v${RENODE_VERSION}/renode-${RENODE_VERSION}.linux-portable.tar.gz \
    && mkdir -p /opt/renode \
    && tar xzf renode-${RENODE_VERSION}.linux-portable.tar.gz -C /opt/renode --strip-components=1 \
    && rm renode-${RENODE_VERSION}.linux-portable.tar.gz

# Create working directories
RUN mkdir -p \
    /workspace/firmware \
    /workspace/firmware_seed \
    /workspace/problem \
    /workspace/problem_seed \
    /workspace/build \
    /workspace/output \
    /workspace/renode

WORKDIR /workspace

COPY docker/scripts/ /workspace/scripts/
COPY renode/ /workspace/renode/
COPY firmware/ /workspace/firmware/
COPY firmware/ /workspace/firmware_seed/
COPY ide/problem/ /workspace/problem/
COPY ide/problem/ /workspace/problem_seed/
COPY --from=report-build /app/dist/index.html /workspace/scripts/report_template.html
RUN chmod +x /workspace/scripts/*.sh

ENTRYPOINT ["/bin/bash"]
