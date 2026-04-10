#!/usr/bin/env bash

# Ensure script runs as root (sudo).
if [ "$(id -u)" -ne 0 ]; then
  echo "Missing permissions. Run with sudo!"
  exit 1
fi

REPO_URL="https://github.com/idetor/Idet.git"
INSTALL_DIR="./idetor"

# Create install directory if needed
mkdir -p "$INSTALL_DIR"

cd "$INSTALL_DIR" || exit 1

echo "Installing dependencies..."
apt update
apt install -y git libncurses-dev libcurl4-openssl-dev nlohmann-json3-dev g++ cmake

# Clone if repo not present, otherwise update
if [ ! -d ".git" ]; then
  echo "Repo not found → cloning..."
  rm -rf "$INSTALL_DIR"/*
  git clone "$REPO_URL" .
else
  echo "Repo exists → updating..."
  git pull
fi

echo "Compiling main.cpp to idet..."
g++ -std=c++20 main.cpp -lncursesw -lcurl -o idet

chmod +x idet
cp -f idet /usr/local/bin/

echo "Installation complete. Run 'idet' to start the editor."