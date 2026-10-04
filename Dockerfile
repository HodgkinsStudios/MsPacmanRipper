# Created by Jacob Hodgkins
# syntax=docker/dockerfile:1

FROM debian:bookworm-slim AS build

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        cmake \
        g++ \
        make \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY CMakeLists.txt /src/CMakeLists.txt
COPY src /src/src

RUN cmake -S /src -B /build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build /build --config Release --parallel

FROM debian:bookworm-slim AS runtime

ARG MSPACMANRIPPER_VERSION=dev

LABEL org.opencontainers.image.title="MsPacmanRipper" \
      org.opencontainers.image.description="Cross-platform Ms. Pac-Man ROM/PROM disassembly tool" \
      org.opencontainers.image.source="https://github.com/HodgkinsStudios/MsPacmanRipper" \
      org.opencontainers.image.licenses="MIT" \
      org.opencontainers.image.version="${MSPACMANRIPPER_VERSION}"

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        python3 \
        unzip \
    && rm -rf /var/lib/apt/lists/*

ENV PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1 \
    HOME=/tmp

WORKDIR /opt/mspacripper
COPY --from=build /build/bin/MsPacmanRipper /opt/mspacripper/bin/MsPacmanRipper
COPY scripts /opt/mspacripper/scripts
COPY evidence /opt/mspacripper/evidence
COPY README.md LICENSE LEGAL.md VERSION /opt/mspacripper/

RUN chmod 0755 /opt/mspacripper/bin/MsPacmanRipper

WORKDIR /work

ENTRYPOINT ["/opt/mspacripper/bin/MsPacmanRipper"]
CMD ["--help"]
