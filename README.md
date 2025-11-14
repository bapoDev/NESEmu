# NESEmu - A simple NES emulator

## The objective

This project was created as a challenge to understand other architectures and reproduce their behavior up to a certain accuracy level. The main objective is to run Super Mario Bros. which is where the guide I follow stops. Then I'll try to run a maximum amount of games with other documentations I can find on the internet.

## Advancement

Currently, the project can emulate a few instructions, mainly branches, stack related and register operations.
It only runs a few test ROMs that uses a basic set of instructions.
There is currently no display, if you want to see the results of the ROMs you play with, you will need a debugger.
Currently the UI is confusing, it is made with basic rectangles in SDL, it will be changed but it's not the priority at all.
The emulator only supports NTSC ROMs for now.

## Build

You will need CMake, SDL3 and a C++ compiler.

First, clone the repo and go to the build directory :

```bash
git clone github.com/bapoDev/NESEmu.git
cd NESEmu
mkdir build && cd build
```

Then, run the following CMake command to configure the project :
```bash
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```

OR outside of the build directory, after configuring the project :
```bash
cmake --build build
```

Then, run the emulator with :
```bash
cd build
./nesemu
```

## Resources and credits

[The guide by 100th Coin](https://www.patreon.com/posts/making-your-nes-137873901)
[The CPU documentation and resources](http://www.6502.org)
