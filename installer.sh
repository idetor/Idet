#!/usr/bin/env bash
# Ensure script runs as root (sudo).
if [ "$(id -u)" -ne 0 ]; then
  echo "Missing permissions. Run with sudo!"
  exit 1
fi

REPO_URL="https://github.com/idetor/Idet.git"
INSTALL_DIR="."


echo "Chose installation method:"
echo "Install from downloaded/Install from repo(might not work)/Update?"
read -rp "Chose: (install/install-repo/update) i/ir/u: " CHOICE
if [[ "$CHOICE" == "update" || "$CHOICE" == "u" || -z "$CHOICE" ]]; then
  echo "Updating from repo..."
  
  SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
  cd "$SCRIPT_DIR" || exit 1
  git pull
  echo "Creating config"
  touch config.json
  echo "Compiling main.cpp to idet..."    
  g++ -std=c++20 main.cpp -lncursesw -lcurl -o idet
  
  chmod a+x idet
  cp -f idet /usr/local/bin/
  echo "Installation complete. Run 'idet' or './idet' to start the editor."

elif [[ "$CHOICE" == "install-repo" || "$CHOICE" == "ir" ]]; then
  echo "Installing from repo..."
  # Create install directory if needed
  mkdir -p "$INSTALL_DIR"
  cd "$INSTALL_DIR" || exit 1
  echo "Installing dependencies..."
  apt update
  apt install -y git libncurses-dev libcurl4-openssl-dev nlohmann-json3-dev g++ cmake
  # Clone if repo not present, otherwise update
  if [ ! -d ".git" ]; then
  echo "Repo not found → cloning..."
  git clone "$REPO_URL" .
  else
  echo "Repo exists → updating..."
  git pull
  fi
  echo "Creating config"
  touch config.json
  echo "Compiling main.cpp to idet..."
  g++ -std=c++20 main.cpp -lncursesw -lcurl -o idet
  chmod +x idet
  cp -f idet /usr/local/bin/
  echo "Installation complete. Run 'idet' to start the editor."

elif [[ "$CHOICE" == "install" || "$CHOICE" == "i" ]]; then
  echo "Installing from downloaded files..."
  echo "Installing dependencies..."
  apt update
  apt install -y git libncurses-dev libcurl4-openssl-dev nlohmann-json3-dev g++ cmake
  echo "Creating config"
  touch config.json
  echo "Compiling main.cpp to idet..."
  g++ -std=c++20 main.cpp -lncursesw -lcurl -o idet
  chmod +x idet
  cp -f idet /usr/local/bin/
  echo "Installation complete. Run 'idet' to start the editor."
  
else
  echo "Invalid choice. Exiting."
  exit 1
fi