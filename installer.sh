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

read -p "Install with llama.cpp? (y/n) " -n 1 -r
echo

if [[ $REPLY =~ ^[Yy]$ ]]; then
    read -p "Do you have llama.cpp installed? (y/n) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        read -p "Enter the path to llama.cpp: " -r
        LLAMA_PATH="$REPLY"
    else
        echo "Installing llama.cpp."
        git clone https://github.com/ggml-org/llama.cpp.git
    
        LLAMA_PATH="$SCRIPT_DIR/llama.cpp"
        cd llama.cpp
        git pull
        cmake -B build
        cmake --build build --config Release

        echo "llama.cpp installalled"
        cd ..
        exit 0
    fi
    echo "Compiling main.cpp to editor with llama.cpp..."
    g++ main.cpp src/LlamaClient.cpp -I./headers -I"$LLAMA_PATH/include" -L"$LLAMA_PATH" -lllama -lncurses -lcurl -o editor
    chmod a+x editor
    echo "Installation complete. Run './editor' to start the editor."
    exit 0
fi

if [[ $REPLY =~ ^[Nn]$ ]]; then
    echo "Skipping llama.cpp installation."
    echo "Compiling main.cpp to editor..."

    g++ main.cpp  -lncurses -lcurl -o editor

    chmod a+x editor
    echo "Installation complete. Run './editor' to start the editor."
    exit 0
fi