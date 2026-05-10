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
ARG PICO_SDK_VERSION=1.5.1

ENV DEBIAN_FRONTEND=noninteractive
ENV PICO_SDK_PATH=/opt/pico-sdk
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

# Clone Pico SDK
RUN git clone --depth 1 --branch ${PICO_SDK_VERSION} \
    https://github.com/raspberrypi/pico-sdk.git ${PICO_SDK_PATH} \
    && cd ${PICO_SDK_PATH} \
    && git submodule update --init --depth 1

# Clone community RP2040 Renode support and use source compilation
# (the repo does not ship precompiled DLLs)
RUN git clone --depth 1 https://github.com/matgla/Renode_RP2040.git /opt/renode-rp2040 \
    && sed -i 's|initialize_peripherals\.resc|initialize_peripherals_source.resc|' \
        /opt/renode-rp2040/boards/initialize_custom_board.resc \
    && find /opt/renode-rp2040/emulation -name '*.cs' -exec \
        sed -i \
            -e 's/LimitTimer(machine\.ClockSource, Frequency,/LimitTimer(machine.ClockSource, (long)Frequency,/g' \
            -e 's/timer\.Frequency = (ulong)/timer.Frequency = (long)/g' \
            -e 's/limit: 0xffffffffffffffff/limit: (long)0x7fffffffffffffff/g' \
        {} +

# Create working directories
RUN mkdir -p /workspace/firmware /workspace/build /workspace/output

WORKDIR /workspace

COPY docker/scripts/ /workspace/scripts/
COPY --from=report-build /app/dist/index.html /workspace/scripts/report_template.html
RUN chmod +x /workspace/scripts/*.sh

ENTRYPOINT ["/bin/bash"]
