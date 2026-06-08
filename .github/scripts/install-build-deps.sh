#!/usr/bin/env bash
# Install Ubuntu build dependencies matching root CMakeLists.txt find_package() calls.
set -euo pipefail

sudo apt-get update
sudo apt-get install -y \
  build-essential \
  cmake \
  qt6-base-dev \
  qt6-base-private-dev \
  qt6-base-dev-tools \
  qt6-declarative-dev \
  qt6-declarative-private-dev \
  qt6-declarative-dev-tools \
  libqt6sql6-sqlite \
  libgl1-mesa-dev \
  libxkbcommon-dev \
  zlib1g-dev \
  libarchive-dev \
  qtkeychain-qt6-dev \
  zip \
  unzip \
  p7zip-full
