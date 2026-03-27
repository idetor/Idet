#!/usr/bin/env bash

# Ensure script runs as root (sudo).
if [ "$(id -u)" -ne 0 ]; then
  echo "Missing permissions. Run with sudo!"
  exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR" || exit 1

apt update
apt install -y libncurses-dev
apt install -y libcurl4-openssl-dev
apt install -y nlohmann-json3-dev
apt install -y g++
apt install -y cmake

git pull

echo "Compiling main.cpp to editor..."
    
g++ main.cpp  -lncursesw -lcurl -o idet
    
chmod a+x idet
cp -f idet /usr/local/bin/
echo "Installation complete. Run 'idet' or './idet' to start the editor."