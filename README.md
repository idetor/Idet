# Idet
Simple terminal text editor.

# Installation
For Ai-Completions(currently unavaiable) you need to install llama.cpp.\
Get llama.cpp via: https://github.com/ggml-org/llama.cpp

Install dependencies:
```bash
sudo apt install libncurses-dev
sudo apt install libcurl4-openssl-dev
sudo apt install nlohmann-json3-dev
```
With llama.cpp:\
Replace the placeholders and run the compiler command:
```bash
g++ main.cpp src/LlamaClient.cpp -I./headers -I"<path-to-llama.cpp>/include" -L"<path-to-llama.cpp>" -lllama -lncurses -lcurl -o editor
```
Without llama.cpp:
```bash
 g++ main.cpp  -lncurses -lcurl -o editor
```


#### Inspired by the Legendary Tedi editor
###### Tedi editor by Micheal Niederle: https://github.com/funkylang/funky_beta


