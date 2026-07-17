FROM docker.io/library/ubuntu:24.04 AS toolchain

ARG RENODE_VERSION=1.16.1
ARG ZEPHYR_VERSION=v4.4.1

ENV DEBIAN_FRONTEND=noninteractive
ENV ZEPHYR_BASE=/opt/zephyrproject/zephyr
ENV ZEPHYR_TOOLCHAIN_VARIANT=zephyr
ENV ZEPHYR_SDK_INSTALL_DIR=/opt/zephyr-sdk
ENV PATH="/opt/zephyr-venv/bin:/opt/renode:${PATH}"

# Ubuntu dependencies follow Zephyr's 24.04 host requirements. The runtime
# also keeps a native compiler for the fast protocol known-answer tests.
RUN apt-get update && apt-get install -y --no-install-recommends \
    bash \
    ca-certificates \
    ccache \
    cmake \
    curl \
    device-tree-compiler \
    file \
    gcc \
    gcc-multilib \
    g++-multilib \
    git \
    gperf \
    libmagic1 \
    make \
    ninja-build \
    python3 \
    python3-dev \
    python3-pip \
    python3-setuptools \
    python3-venv \
    python3-wheel \
    wget \
    xz-utils \
    && rm -rf /var/lib/apt/lists/*

# Stable Renode release. The bundled nRF52840 model and BLEMedium are used
# directly; no community board fork is needed for this scenario.
RUN wget -q \
      "https://github.com/renode/renode/releases/download/v${RENODE_VERSION}/renode-${RENODE_VERSION}.linux-portable.tar.gz" \
      -O /tmp/renode.tar.gz \
    && mkdir -p /opt/renode \
    && tar xzf /tmp/renode.tar.gz -C /opt/renode --strip-components=1 \
    && rm /tmp/renode.tar.gz \
    && renode --version

# Pin a shallow Zephyr manifest checkout, then fetch only the three modules used
# by an nRF52840 BLE build. `west update --narrow` fetches the pinned revisions
# without every remote branch and tag. Base requirements are sufficient for
# building and for the SDK installer; test, flashing, and compliance packages
# would add hundreds of unrelated tools to the runtime image.
RUN python3 -m venv /opt/zephyr-venv \
    && pip install --no-cache-dir west \
    && mkdir -p /opt/zephyrproject \
    && git clone --depth 1 --branch "${ZEPHYR_VERSION}" \
         https://github.com/zephyrproject-rtos/zephyr \
         /opt/zephyrproject/zephyr \
    && west init -l /opt/zephyrproject/zephyr \
    && cd /opt/zephyrproject \
    && west update --narrow cmsis cmsis_6 hal_nordic \
    && west zephyr-export \
    && pip install --no-cache-dir \
         -r /opt/zephyrproject/zephyr/scripts/requirements-base.txt \
    && cd /opt/zephyrproject/zephyr \
    && west sdk install --install-dir /opt/zephyr-sdk \
         --gnu-toolchains arm-zephyr-eabi

FROM toolchain AS gateway-protocol-builder

COPY reference/firmware/ghost_protocol.c /tmp/gateway-protocol/ghost_protocol.c
COPY reference/firmware/include/ghost_protocol.h /tmp/gateway-protocol/include/ghost_protocol.h
RUN /opt/zephyr-sdk/gnu/arm-zephyr-eabi/bin/arm-zephyr-eabi-gcc \
        -std=c17 -Os -ffunction-sections -fdata-sections \
        -fno-common -mcpu=cortex-m4 -mthumb -mabi=aapcs \
        -I /tmp/gateway-protocol/include \
        -c /tmp/gateway-protocol/ghost_protocol.c \
        -o /tmp/gateway-protocol/ghost_protocol.o

FROM toolchain AS participant-runtime

ARG VCS_REF=unknown

LABEL org.opencontainers.image.title="GhostTag Apocalypse participant runtime" \
      org.opencontainers.image.revision="${VCS_REF}"

# Challenge Jobs intentionally have no Internet access. Cache the SVD in a
# small runtime layer so Renode never waits on one download per emulated board.
RUN mkdir -p /opt/renode/svd \
    && wget -q \
      "https://dl.antmicro.com/projects/renode/svd/NRF52840.svd.gz" \
      -O /opt/renode/svd/NRF52840.svd.gz \
    && sed -i \
      's|@https://dl.antmicro.com/projects/renode/svd/NRF52840.svd.gz|@/opt/renode/svd/NRF52840.svd.gz|' \
      /opt/renode/platforms/cpus/nrf52840.repl

RUN mkdir -p /workspace/build /workspace/output /workspace/firmware \
    /workspace/firmware_seed /workspace/problem_seed /workspace/support \
    /workspace/renode

WORKDIR /workspace

COPY --chmod=0755 docker/scripts/ /workspace/scripts/
COPY firmware/ /workspace/firmware_seed/
COPY ide/problem/ /workspace/problem_seed/
COPY support/ /workspace/support/
COPY --from=gateway-protocol-builder /tmp/gateway-protocol/ghost_protocol.o \
    /workspace/support/gateway/ghost_protocol.o

ENTRYPOINT ["/bin/bash", "/workspace/scripts/entrypoint.sh"]
