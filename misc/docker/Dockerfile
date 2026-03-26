# syntax=docker/dockerfile:1
#
# Dockerfile for building pottendo-Pi1541
#
# Layer structure:
#   Layer 1: Base OS + packages           - cached indefinitely
#   Layer 2: ARM cross-compilers          - cached indefinitely
#   Layer 3: COPY build.sh + patches      - invalidated only when those files change
#   Layer 4: build.sh -c                  - cached as long as layer 3 doesn't change
#   Layer 5: COPY full source + .git/     - invalidated on any source change
#
# The Pi1541 build happens at docker run time via ENTRYPOINT,
# allowing the target architecture to be passed from the host:
#
#   docker run --rm pottendo-pi1541-builder              # all architectures
#   docker run --rm pottendo-pi1541-builder -a pi4-64   # single architecture
#
# Build image: docker build -t pottendo-pi1541-builder .
# See docker_build.md for full usage.

FROM debian:bookworm-slim

# Layer 1: system packages
# vice (provides c1541) is in the non-free repository
# gcc/g++ are required by circle-stdlib's configure script to build host-side tools
RUN echo "deb http://deb.debian.org/debian bookworm main contrib non-free non-free-firmware" > /etc/apt/sources.list \
    && apt-get update && apt-get install -y --no-install-recommends \
    make wget unzip git sed patch ca-certificates xz-utils vice dos2unix \
    gcc g++ texinfo \
    && rm -rf /var/lib/apt/lists/*

# Layer 2: ARM cross-compilers
RUN wget -q https://developer.arm.com/-/media/Files/downloads/gnu/14.3.rel1/binrel/arm-gnu-toolchain-14.3.rel1-x86_64-arm-none-eabi.tar.xz \
        -O /tmp/arm32.tar.xz \
    && tar -xf /tmp/arm32.tar.xz -C /opt/ \
    && rm /tmp/arm32.tar.xz

RUN wget -q https://developer.arm.com/-/media/Files/downloads/gnu/14.3.rel1/binrel/arm-gnu-toolchain-14.3.rel1-x86_64-aarch64-none-elf.tar.xz \
        -O /tmp/arm64.tar.xz \
    && tar -xf /tmp/arm64.tar.xz -C /opt/ \
    && rm /tmp/arm64.tar.xz

ENV PATH="/opt/arm-gnu-toolchain-14.3.rel1-x86_64-arm-none-eabi/bin:/opt/arm-gnu-toolchain-14.3.rel1-x86_64-aarch64-none-elf/bin:$PATH"

# Layer 3: copy only the files needed by build.sh -c
# Keeping this separate from the full COPY prevents build.sh -c from
# re-running on every source change.
WORKDIR /build/pottendo-Pi1541

COPY build.sh options.txt config.txt ./
COPY src/Circle/ ./src/Circle/
COPY CBM-FileBrowser_v1.6/ ./CBM-FileBrowser_v1.6/

RUN dos2unix build.sh

# Minimal git repo so build.sh -c passes its remote URL sanity check.
# The real .git/ is copied in layer 5 and overwrites this before build.sh runs.
RUN git init && git remote add origin https://github.com/pottendo/pottendo-Pi1541.git

# Layer 4: clone circle-stdlib, apply patches, fetch ROMs/firmware, build CBM-FileBrowser
# Populates /build/circle-stdlib and /build/Pi-Bootpart (ROMs, firmware, boot files)
RUN ./build.sh -c

# Layer 5: copy full source including .git/
# .git/ is required for:
#   - build.sh remote URL sanity check (git remote get-url origin)
#   - version string generation (git describe --tags -> src/version.h)
COPY . /build/pottendo-Pi1541

RUN find /build/pottendo-Pi1541 -name "*.sh" | xargs dos2unix

# Pi1541 build happens at docker run time so the target architecture
# can be passed as an argument from the host.
ENTRYPOINT ["./build.sh"]
CMD []
