#!/usr/bin/env bash

# Ensure script runs as root (sudo).
if [ "$(id -u)" -ne 0 ]; then
  echo "Missing permissions. Run with sudo!"
  exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR" || exit 1

apt update
apt install -y git libncurses-dev libcurl4-openssl-dev nlohmann-json3-dev g++ cmake


git pull

echo "Compiling main.cpp to idet..."
    
g++ -std=c++20 main.cpp -lncursesw -lcurl -o idet
  

#mkdir -p ~/.config/idet
#echo "{}" > ~/.config/idet/config.json

chmod a+x idet
cp -f idet /usr/local/bin/
echo "Installation complete. Run 'idet' or './idet' to start the editor."

