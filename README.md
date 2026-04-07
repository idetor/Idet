# Idet
Simple terminal text editor.

# Installation
## Install via the installer:
```bash
sudo chmod a+x ./installer.sh
./installer.sh
```
## Manual install:
Install dependencies:
```bash
sudo apt install libncurses-dev
sudo apt install libcurl4-openssl-dev
sudo apt install nlohmann-json3-dev
sudo apt install g++
```
Compile:
```bash
g++ -std=c++20 main.cpp -lncursesw -lcurl -o idet
```


#### Inspired by the Legendary Tedi editor
###### Tedi editor by Micheal Niederle: https://github.com/funkylang/funky_beta


