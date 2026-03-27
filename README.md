# Idet
Simple terminal text editor.

# Installation
Replace the placeholders and run the compiler command:
```bash
g++ main.cpp src/LlamaClient.cpp -I./headers -I"<path-to-llama.cpp>/include" -L"<path-to-llama.cpp>" -lllama -lncurses -lcurl -o editor
```

## Inspired by the Legendary Tedi editor
Tedi editor by Micheal Niederle: https://github.com/funkylang/funky_beta