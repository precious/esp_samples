FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    bc \
    binutils \
    bzip2 \
    ca-certificates \
    cpio \
    diffutils \
    file \
    findutils \
    gawk \
    git \
    gzip \
    libncurses-dev \
    make \
    patch \
    perl \
    python3 \
    rsync \
    sed \
    tar \
    unzip \
    wget \
    xz-utils \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /work
CMD ["bash"]
