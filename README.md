# UMJIT

A Just-In-Time compiler from Universal Machine assembly language to x86 machine code.
Compiles and executes a UM program 93% faster than an optimized UM emulator.

## Overview

The Universal Machine (or UM) is an simple virtual machine that all CS students at Tufts implement in CS40: Machine Structure and Assembly Language Programming. The UM has 8 32-bit registers, recognizes 14 instructions, and has 32-bit word-oriented memory. One special memory segment contains the current UM "program" that is being executed. The memory of the UM is bounded only by the memory constraints of the host machine.

Students implement the UM as an emulator with a stack allocated array to store UM register contents and heap allocated memory segments.

This implementation just-in-time compiles UM instructions to x86 machine code and executes it inline. The program result is exactly the same as a Universal Machine emulator, but runs 93% faster than my already aggressively profiled emulator.

## Design

### Emulator
I built a UM emulator in Spring 2023 when I took CS40. I modified it slightly to run in native C with no external dependencies.
The source code can be found in /x86container/docker_shared/emulator/emulator.c

The emulator uses a stack-allocated fixed size array to store register values and uses a dynamic array to store memory segment pointers.
It unpacks 32-bit UM instructions and uses a big conditional statement to executes them based on the opcode and register/value contents.

To see what UM programs the emulator can run, please see my demo video linked (HERE);

### Just-In-Time Compiler



#### Why x86?
I chose x86_64 linux for two reasons. Mostly, I wanted to be able to run my program on the Tufts CS department servers, which run x86 linux. Second, I was already familiar with x86 assembly syntax, so writing the assembly instructions as psuedocode for the machine code made a lot of sense.

It's worth noting that the UM instruction set is so simple that the machine code it compiles to is simple as well. Most of the x86 instructions that get executed are simple movs, adds, mults, divs, ands, nots, loads, and stores. By far the most expensive operations are function calls.

## Performance

I timed my program in two environments.
First was in a docker container running x86_64 linux on my apple silicon mac
The second was on my student VM on the Tufts department servers running x86_64 redhat linux

The apple hardware is significantly faster than my VM, even with the ARM hardware emulating x86. To get an idea of how much slower
emulated x86 is than native ARM, I modified the emulator to run in native C and ran the program in 3 different environments
1. Native MacOS (which forced me to compile with clang: gcc is my compiler of choice)
2. A docker container running Aarch64 ubuntu linux 
3. A docker container running x86 ubuntu linux (I was planning to use redhat, but some utilities I needed weren't available)

The emulator ran the sandmark.umz benchmark (a UM program containing over 2 million instructions) in:
1. 2.50 seconds on MacOS
2. 2.29 seconds in the Aarch64 container (which is compatile with Apple's ARM architecture)
3. 2.77 seconds in the x86 container (which is not compatible with Apple's ARM architecture and has to run the rosetta emulator)

Wait, what? Yup, you read that right. The program ran faster in a docker container running linux on the Mac than it did just running natively on MacOS. The Aarch container run was achieved by compiling with gcc using the -O1 flag. Using clang and/or -O2 resulted in a
2.5 second runtime, so gcc found the sweet spot here with -01. On macOS, gcc wasn't available, so I was forced to compile with clang, and found -O2 to be the best optimization. In the x86 docker container, I found best results compiling with gcc and -O2.

Based on these initial tests, I concluded 3 things:
1. Apple makes excellent hardware
2. The emulated x86_64 architecture running on the Apple hardware, is about 20% slower than the native ARM architecture
3. Amazingly, Docker runs Aarch linux on Apple hardware faster than MacOS runs on Apple hardware

Here is the perfomance comparisons between the emulator and the JIT.

In my x86 container:
### Emulator
Midmark: 0.13 seconds
Sandmark: 2.77 seconds

### JIT Compiler
Midmark: 0.10 seconds
Sandmark: 1.45 seconds

On my Tufts student VM:
Emulator:
TODO

JIT Compiler:
TODO

## Running the Program
1. Download the source code
2. Build or download my docker image:
Unless your computer runs x86_64 linux (and maybe even if it does!), you will likely want to run the program with docker.
You can download my docker image from docker hub (HERE), or you can build it yourself using my docker file.
```docker build -t dev-tools-x86 .```
The container has the bare minimum utilities you need to run the program. It also accesses the `docker_shared` directory, which
is shared between the container and your local machine

```docker run something something shared directory```

TODO: can you load a docker container with files already inside of it?

3. Navigate to the x86container/docker_shared directory
4. Compile with `make`
5. Run with `./jit umasm/sandmark.umz`
The umasm directory contains many UM programs.

## Challenges
This program is of course not portable, as it executes x86 machine code that will not run on another system.

## Potential Improvements
The benchmark assembly language programs used to test emulators have a couple weaknesses that I exploited to make my JIT faster.

## Acknowledgements
Mark Sheldon for giving me the idea to make a compiler for the UM.
Peter Wolfe, my project partner for the UM assignment.
Norman Ramsey, the author of the UM assignment.



 
