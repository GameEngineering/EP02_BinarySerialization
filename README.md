## Binary Serialization and Pixel Art in C and OpenGL | Game Engineering - EP02

<p align="left">
  <img width="460" height="300" src="assets/vid_thumb.png">
</p>

https://www.youtube.com/watch?v=GXdT8twQxxI&t=0s

A small, custom pixel art editor using C and OpenGL showing off binary serialization techniques. 

## Building

**NOTE(john)**: Currently requires at least **OpenGL v3.3** to run. This will be addressed in the future to allow for 
          previous opengl versions. 

  - **windows(msvc)**:
    - From start menu, search for "x64/x86 Native Tool Command Prompt for {Insert your Version Here}"
    - Navigate to where you have `EP02_BinarySerialization` repo placed
    - run `proc\win\compile_win_cl.bat`
    - The executable will be placed in `bin\`
    - run `bin\App.exe`
  - **windows(mingw)**:
    - From gitbash, `cd` into root directory for this project
    - run `bash ./proc/win/mingw.sh`
    - The executable will be placed in `bin/`
    - run `./bin/App.exe`
    - **html5(emscripten)**:
    - You'll need emscripten compiler
    - From gitbash, `cd` into root directory for this project
    - run `bash ./proc/html5/emcc.sh`
    - The executable will be placed in `bin/`
    - run `./bin/App.exe`
  - **osx**:
    - You'll need gcc
    - From the terminal, `cd`into the root directory for this project.
    - run `bash ./proc/osx/compile_osx_gcc.sh`
    - The exectuable will be placed in `bin/`
    - run `./bin/App`
  - **linux**: 
    - You'll need gcc
    - From the terminal, `cd` into the root directory for this project.
    - run `bash ./proc/linux/compile_linux_gcc.sh`
    - The exectuable will be placed in `bin/`
    - run `./bin/App`

## Controls
  - `[`: Decrease paint tool radius
  - `]`: Increase paint tool radius
  - `c`: Clear canvas of all particles
  - `esc`: Quit application
  - Left click mouse to paint

