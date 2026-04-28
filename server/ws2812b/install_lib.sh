#!/bin/bash
# Install rpi_ws281x from source (works on Pi OS Bookworm / Pi 3/4/5)
set -e

echo "=== Installing build dependencies ==="
sudo apt update
sudo apt install -y git cmake build-essential

echo "=== Cloning rpi_ws281x ==="
cd /tmp
rm -rf rpi_ws281x
git clone https://github.com/jgarff/rpi_ws281x.git
cd rpi_ws281x

echo "=== Building ==="
mkdir -p build && cd build
cmake -D BUILD_SHARED=OFF -D BUILD_TEST=OFF ..
make -j$(nproc)

echo "=== Installing to /usr/local ==="
sudo make install
sudo ldconfig

echo ""
echo "Done! Now go to your project folder and run: make"
