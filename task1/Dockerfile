# ---- Stage 1: сборка ----
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        build-essential && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /build
COPY . .
RUN make

# ---- Stage 2: финальный образ ----
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        diffutils && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=builder /build/file_copier /app/file_copier
COPY test.sh /app/test.sh
RUN chmod +x /app/test.sh

ENTRYPOINT ["/app/file_copier"]