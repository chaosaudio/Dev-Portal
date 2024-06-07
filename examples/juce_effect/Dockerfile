# How to use
# docker buildx build --platform linux/arm/v7 -t chaos-audio-builder --load .
# docker run --rm -it -u $UID --platform linux/arm -v "$(pwd):/workdir" chaos-audio-builder # Cross-compilation x86_64 to arm/v7
# docker run --rm -it --platform linux/arm -v "%cd%:/workdir" chaos-audio-builder

FROM debian:bullseye-slim

ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update && apt-get install -y apt-utils build-essential gcc g++ cmake

WORKDIR /workdir

COPY entrypoint.sh /
RUN chmod +x /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
