ARG BASE=debian:trixie-slim
FROM ${BASE}
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential pkg-config python3 python3-venv python3-pip python3-pycotap \
    meson ninja-build flex bison libglib2.0-dev libpixman-1-dev zlib1g-dev \
    curl ca-certificates xz-utils patch libvulkan1 mesa-vulkan-drivers \
    vulkan-validationlayers spirv-tools && rm -rf /var/lib/apt/lists/*
