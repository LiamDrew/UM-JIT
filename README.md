# UMJIT

A Just-In-Time compiler from Universal Machine assembly language to x86 machine code.
Compiles and executes a UM program 2.8 times faster than an optimized UM emulator. Video demo [(HERE)](https://www.youtube.com/watch?v=aWdoqx7MhJY&ab_channel=LiamDrew).

## Overview

The Universal Machine (or UM) is an simple virtual machine that all CS students at Tufts implement in CS40: Machine Structure and Assembly Language Programming. The UM has 8 32-bit registers, recognizes 14 instructions, and has 32-bit word-oriented memory. One special memory segment contains the current UM "program" that is being executed. The memory of the UM is bounded only by the memory constraints of the host machine.

Students implement the UM as an emulator with a stack allocated array to store UM register contents and heap allocated memory segments.

This implementation just-in-time compiles UM instructions to x86 machine code and executes it inline. The program result is exactly the same as a Universal Machine emulator, but runs 2.8 times faster than my already aggressively profiled emulator.

## Running the Program
1. Download the source code
2. Navigate to the ```x86container``` directory
2. Build or download my docker image
Unless your machine runs x86_64 linux (and maybe even if it does), you will likely want to run the program in a docker container. After you install docker, you can download my docker image from Docker Hub, or you can build it yourself using my docker file.

To download my image:
```docker pull liamdrew92/dev-tools-x86:latest```

To build my image yourself:
```docker buildx build --platform=linux/amd64 -t dev-tools-x86 .```
The container has the utilities you need to run the program. It also accesses the `docker_shared` directory, which is shared between the container and your machine.

3. Start the docker container
If you built the image with the Dockerfile, run the container with:
```docker run --platform=linux/amd64 -it -v "$PWD/docker_shared:/home/developer/shared" dev-tools-x86```

If you downloaded the image from Dockerhub, run with:
```docker run --platform=linux/amd64 -it -v "$PWD/docker_shared:/home/developer/shared" liamdrew92/dev-tools-x86:latest```

4. Navigate to the ```shared``` directory
4. Compile with `make`
5. Run with `./jit umasm/sandmark.umz`

The umasm directory contains many UM programs. Sandmark is a benchmark containing 2 million instructions that is designed to stress test the UM.

## Design

### Emulator
I built a UM emulator in Spring 2023 when I took CS40. I modified it slightly to run in native C with no external dependencies.
The source code can be found in /x86container/docker_shared/emulator/emulator.c

The emulator uses a stack-allocated fixed size array to store register values and uses a dynamic array to store memory segment pointers.
It unpacks 32-bit UM instructions and uses a big conditional statement to executes them based on the opcode and register/value contents.

### Just-In-Time Compiler

The key component of the JIT is that it uses the r8 - r15 machine registers to represent the r0 - r7 UM registers. The idea is that a UM instruction that adds two registers can be compiled to only a few bytes of machine code, and can therefore complete extremely fast. The entire program is built around the choice to use machine registers to represent UM registers. Most UM instructions are compiled into pure x86 machine code, but a few more complex ones (map segment, unmap segment, and load program) are compiled into machine code that loads the address of a C function and calls it inline.


#### Why x86?
I chose x86_64 linux for two reasons. Mostly, I wanted to be able to run my program on the Tufts CS department servers, which run x86 linux. Second, I was already familiar with x86 assembly syntax, so writing the assembly instructions as psuedocode for the machine code made a lot of sense.

It's worth noting that the UM instruction set is so simple that the machine code it compiles to is simple as well. Most of the x86 instructions that get executed are simple movs, adds, mults, divs, ands, nots, loads, stores, and function calls.

## Performance

I timed my program in two environments.
First was in a docker container running x86_64 linux on my apple silicon mac
The second was on my student VM on the Tufts department servers running x86_64 redhat linux.

Here is the perfomance comparisons between the emulator and the JIT in these environemnts

### x86 Container:
#### Emulator:
Midmark: 0.13 seconds  
Sandmark: 2.8 seconds  

#### JIT Compiler:
Midmark: 0.09 seconds  
Sandmark: 1.01 seconds  

### Tufts student VM:
#### Emulator:
Midmark: 0.19 seconds  
Sandmark: 5.10 seconds  

#### JIT Compiler:
Midmark: 0.14 seconds  
Sandmark: 3.63 seconds  

The apple hardware is significantly faster than my VM, even with the ARM hardware emulating x86. To get an idea of how much slower emulated x86 is than native ARM, I modified the emulator to run in native C and ran the program in 3 different environments
1. Native MacOS (which forced me to compile with clang; gcc is my compiler of choice)
2. A docker container running Aarch64 Ubuntu linux 
3. A docker container running x86_64 Ubuntu linux (I was planning to use redhat, but some utilities I needed weren't available)

The emulator ran the sandmark.umz benchmark (a UM program containing over 2 million instructions) in:
1. 2.50 seconds on MacOS
2. 2.29 seconds in the Aarch64 container (which is compatile with Apple's ARM architecture)
3. 2.80 seconds in the x86 container (which is not compatible with Apple's ARM architecture and has to run the rosetta emulator)

Wait, what? Yup, you read that right. The program ran faster in a docker container running linux on the Mac than it did just running natively on MacOS. The Aarch container run was achieved by compiling with gcc using the -O1 flag. Using clang and/or -O2 resulted in a 2.5 second runtime, so gcc found the sweet spot here with -01. On macOS, gcc wasn't available, so I was forced to compile with clang, and found -O2 to be the best optimization. In the x86 docker container, I found best results compiling with gcc and -O2.

Based on these initial tests, I concluded 3 things:
1. Apple makes excellent hardware
2. The emulated x86_64 architecture running on the Apple hardware, is about 20% slower than the native ARM architecture
3. Amazingly, Docker runs Aarch linux on Apple hardware faster than MacOS runs on Apple hardware. (Some of this is likely due to gcc being better optimized than clang).

## Challenges
This program is (of course) not portable, as it executes x86 machine code that will not run on another system.

## Potential Improvements and Considerations
The benchmark assembly language programs used to test emulators have a couple weaknesses that I exploited to make my JIT faster. If a UM program were to encounter a segmented store that stored an instruction into the zero segment that was going to be executed, this compiler would crash. However, neither the midmark or the sandmark demand this of the JIT. To handle this case, any instruction stored in the zero segment would have to be compiled into machine code, even if it never ends up being executed. This slows the program down, so I removed it from my compiler and am noting the choice here.

## Acknowledgements
Professor Mark Sheldon, for giving me the idea to make a compiler for the UM.
Peter Wolfe, my project partner for the UM assignment.
