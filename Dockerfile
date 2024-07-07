# How to use
# docker buildx build --platform linux/arm -t chaos-audio-builder --load .
# docker run --rm -it -u $UID --platform linux/arm -v "$(pwd):/workdir" chaos-audio-builder # Cross-compilation x86_64 to arm/v7

FROM debian:bullseye-slim

ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /workdir

COPY entrypoint.sh /
RUN chmod +x /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
