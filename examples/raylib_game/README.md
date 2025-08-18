# Raylib Game
This project serves as an example of how to link a C library with Honey and use it. These are **not** actual Raylib bindings.

To run this project, first make sure raylib has been built, you can check its page on [how to build Raylib](https://github.com/raysan5/raylib?tab=readme-ov-file#build-and-installation).
Afterwards, run `honeyc run src/main.hun -Iraylib/src -Lraylib/build/raylib -lraylib -lGL -lm -lpthread -ldl -lrt -lX11`
