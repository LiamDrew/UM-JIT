# UM-JIT

A high performance virtual runtime for Universal Machine programs. Powered by a just-in-time compiler that compiles Universal Machine assembly language to native machine code on Arm64 and x86-64 platforms. Compiles and executes benchmark Universal Machine programs up to 4.5 times faster than an emulator-based runtime. Video demo [here](https://www.youtube.com/watch?v=aWdoqx7MhJY&ab_channel=LiamDrew).

## Overview

The Universal Machine (or UM) is a simple 32 bit virtual machine with a RISC-style instruction set. (In this documentation, the terms Univeral Machine, UM, and virtual machine are used interchangably). The UM has 8 general-purpose 32 bit registers, an instruction pointer, and maps memory segments that are each identified by a 32 bit integer. Much like RISC machine code instructions, each UM machine code instruction is packed in 4 byte "words", with certain bits to identify the opcode, source and destination registers, and values to load into registers. Unlike a typical machine, the UM's memory is oriented around these 4 byte words, and is not byte-addressable.

The UM recognizes 14 instructions:  

Conditional Move  
Addition  
Multiplication  
Division  
Bitwise NAND  
Map Memory Segment  
Unmap Memory Segment  
Load Register (from memory segment)  
Store Register (into memory segment)  
Load Immediate Value into register 
Output register  
Input into register   
Halt  
Load Program (see below)  

One special memory segment mapped by the segment identifier 0 contains the UM machine code instructions that are currently being executed. The load program instruction can jump to a different point in this segment and continue executing, or can duplicate another memory segment and load it into the zero segment to be executed.

It's possible to build surprisingly complex programs from these 14 instructions and execute them in a UM virtual runtime. (By virtual runtime, I mean any virtualized environment in which a UM program can run).

All CS students at Tufts implement a Universal Machine emulator in the Machine Structure course (CS40). After implementing a working UM, students profile their program and modify it to run as fast as possible.

There are many performance gains to be achieved through profiling, but the key limiting bottleneck is that the UM registers are stored in stack memory. This means an emulator must access memory for every single operation, limiting the performance potential of the virtual machine.

### Design Comparison: JIT Compiler vs Emulator

My UM virtual runtime uses just-in-time compilation to translate UM instructions to native machine code. The key component of the JIT compiler is that it uses 8 machine registers to store the contents of the UM registers. This accelerates the virtual runtime for two reasons:  
1. Fewer memory accesses at runtime:  
A pure emulator must access memory for every single operation. Even for a simple instruction like addition, an emulator must access registers in memory, do the addition, and store the result back into memory. Even though this oft-accessed memory lives in the L1 cache for the duration of the program, the memory access still adds a couple cycles of overhead to every single instruction.   
By contrast, the JIT compiler translate the UM addition instruction into a hardware addition instruction, using the machine registers themselves to store the contents of the virtual machine registers. This allows the same UM addition operation to be executed with a single hardware instruction (or 2, depending on the platform, since 3 registers are involved). This keeps more of the workload between registers on the CPU, minimizing the overhead of memory accesses.

2. Instructions are translated to native machine code ahead of time:  
A pure emulator needs a large conditional statement or jump table to decode each UM instruction based on its opcode and branch to the appropriate code for handling the instruction. This requires a lot of jumping around between machine instructions to decode and execute UM instructions.  
By comparison, the JIT compiler translates UM instructions into machine code each time a new program is loaded into the zero (execution) segment. After the translation is complete, the UM can blitz through the compiled machine instructions with minimal branching.  
Most UM instructions are compiled into pure machine code, but the more complex ones (map segment, unmap segment, input, output, and load program) are compiled into machine code that branches to handwritten assembly that subsequently calls a C function. This is done because all UM instructions must get compiled to the same number of bytes of machine code to preserve alignment, so we want to avoid bloating the standard size of a compiled UM instruction.

This JIT-based UM virtual runtime also uses a custom 32 bit memory allocator to accelerate address translations for the Load Register and Store Register instructions. This memory allocator is itself an entire project; see [Virt32](https://github.com/LiamDrew/Virt32) for more information.

Put together, this JIT-based implementation runs UM programs up to 4.58 times faster than an aggressively profiled pure emulator on an Arm-based Macbook, and TODO: 2.8 times faster on an x86 server. Please see the performance section for more details 

## Running the Program
1. Download the source code.  

2. Choose a platform. There are 3 root directories: linux-x86-64-container, linux-arm64-container, and darwin-arm64. As their names suggest, the linux containers run linux on Arm64 and x86-64 platforms using docker. The darwin-arm64 directory doesn't use docker, and is intended to compile natively on an Arm-based Mac running MacOS. For darwin-arm64, skip to step 6.

3. Build the docker image:  
x86-64:  
```docker buildx build --platform=linux/amd64 -t dev-tools-x86 .```  
Arm64 (Aarch64):  
```docker buildx build --platform=linux/arm64 -t dev-tools-aarch64 .```  
Both containers have the utilities you need to run the program. They can each access their own `docker_shared` directory, which is shared between the container and your machine.

4. Start the docker container:  
x86-64:  
```docker run --platform=linux/amd64 -it -v "$PWD/docker_shared:/home/developer/shared" dev-tools-x86```  
Arm64:  
```docker run --platform=linux/arm64 -it -v "$PWD/docker_shared:/home/developer/shared" dev-tools-aarch64```
5. Navigate to the ```shared``` directory

6. Each container root directory contains the following subdirectories: `/emulator`, `/jit`, `/umasm`.  
The `/umasm` directory contains a variety of Universal Machine assembly language programs.  
The `/emulator` directory contains an executable binary of a profiled emulator-based UM virtual runtime. The binary can be run with `./um ../umasm/[program.um]`. The emulator source code is intentionally omitted, since building the emulator is a class project.   
The `/jit` directory contains the executable binary source code for the JIT compiler-based UM virtual runtime.

7. Navigate to the `/jit` directory
8. Compile with `make`
9. Run a benchmark program that executes 2 million UM instructions with `./jit umasm/sandmark.umz`

## Performance

I timed my program in two environments.
First was in a docker container running x86_64 linux on my apple silicon mac
The second was on my student VM on the Tufts department servers running x86_64 redhat linux.


| Runtime | Platform | Best Compiler + Flags | OS     | Hardware | Time (seconds) |
| ------- | -------- | --------------------- | ------ | -------- | -------------- |
| JIT     | Arm64    | clang -O2             | Darwin | M3 Max   | 0.53           |
| JIT     | Arm64    | clang -O2             | Darwin | M3 Max   | 0.53           |



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

## Potential Improvements and Considerations
The benchmark assembly language programs used to test emulators have a couple weaknesses that I exploited to make my JIT faster. If a UM program were to encounter a segmented store that stored an instruction into the zero segment that was going to be executed, this compiler would crash. However, neither the midmark or the sandmark demand this of the JIT. To handle this case, any instruction stored in the zero segment would have to be compiled into machine code, even if it never ends up being executed. This slows the program down, so I removed it from my compiler and am noting the choice here.

## Acknowledgements
Professor Mark Sheldon, for giving me the idea to make a JIT compiler for the Univeral Machine.  

Tom Hebb, for sharing a similar JIT compiler built years ago. Tom's project had several brilliant ideas that helped me make my JIT compiler much faster, most notably the use of hand-written assembly at compile time to compliment the JIT compiled machine code for complex instructions.  

Peter Wolfe, my project partner for the Univeral Machine assignment. 

Milo Goldstein, Jason Miller, Hameedah Lawal, and Yoda Ermias (my JumboHack 2025 teammates) for helping me designing and implement the Virt32 memory allocator for use with the Universal Machine.
