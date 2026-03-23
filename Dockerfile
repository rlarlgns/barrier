FROM --platform=linux/amd64 ubuntu:20.04

# Avoid prompts during installation
ENV DEBIAN_FRONTEND=noninteractive

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    qtbase5-dev \
    libqt5serialport5-dev \
    libavahi-compat-libdnssd-dev \
    libcurl4-openssl-dev \
    libssl-dev \
    libx11-dev \
    libxtst-dev \
    wget \
    file \
    && rm -rf /var/lib/apt/lists/*

# Set Qt path for linuxdeployqt
ENV PATH="/usr/lib/qt5/bin:$PATH"

# Pre-extract linuxdeployqt to avoid AppImage runtime issues in Docker
RUN wget -c -nv "https://github.com/probonopd/linuxdeployqt/releases/download/continuous/linuxdeployqt-continuous-x86_64.AppImage" -O /tmp/linuxdeployqt.AppImage && \
    chmod +x /tmp/linuxdeployqt.AppImage && \
    sed -i 's|AI\x02|\x00\x00\x00|' /tmp/linuxdeployqt.AppImage && \
    cd /opt && \
    /tmp/linuxdeployqt.AppImage --appimage-extract && \
    mv squashfs-root linuxdeployqt && \
    rm /tmp/linuxdeployqt.AppImage

WORKDIR /src
