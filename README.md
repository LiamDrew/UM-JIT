# UM-JIT

A high performance virtual machine for executing Universal Machine programs. Powered by a just-in-time compiler that compiles Universal Machine assembly language to native machine code on Arm64 and x86-64 platforms. Compiles and executes benchmark Universal Machine programs up to 4.5 times faster than an emulator-based runtime.

## Overview

The Universal Machine (or UM) is a simple 32 bit virtual machine with a RISC-style instruction set. (In this documentation, the terms Univeral Machine, UM, and virtual machine are used interchangably). The UM has 8 general-purpose 32 bit registers, an instruction pointer, and maps memory segments that are each identified by a 32 bit integer. Much like real RISC machine code instructions, each UM machine code instruction is packed in 4 byte "words", with certain bits to identify the opcode, source and destination registers, and values to load into registers. Unlike a real computer, the UM's memory is oriented around these 4 byte words and is not byte-addressable.

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

It's possible to build surprisingly complex programs from these 14 instructions and execute them in a UM virtual runtime, including a text-based adventure game and simple OS with a file system. (By virtual runtime, I mean any virtualized environment in which a UM program can run). All CS students in the Machine Structure course at Tufts (CS40) implement an emulator-based runtime for UM programs. After implementing a working UM, students profile their program and modify it to run as fast as possible. There are many performance gains to be achieved through profiling, but the key limiting bottleneck is that the UM registers are stored in memory. This means an emulator must access memory for every single operation, limiting the performance potential of the virtual machine.

### Design Comparison: JIT Compiler vs Emulator

My UM virtual runtime uses just-in-time compilation to translate UM instructions to native machine code. The key component of the JIT compiler is that it uses 8 machine registers to store the contents of the UM registers. This accelerates the virtual runtime for two reasons:  
1. Fewer memory accesses at runtime:  
A pure emulator must access memory for every single operation. Even for a simple instruction like addition, an emulator must access registers in memory, do the addition, and store the result back into memory. Even though this oft-accessed memory lives in the L1 cache for the duration of the program, the memory access still adds a couple cycles of overhead to every single instruction.   
By contrast, the JIT compiler translate the UM addition instruction into a hardware addition instruction, using the machine registers themselves to store the contents of the virtual machine registers. This allows the same UM addition operation to be executed with a single hardware addition instruction. This keeps more of the workload between registers on the CPU, minimizing the overhead of memory accesses.

2. Instructions are translated to native machine code ahead of time:  
A pure emulator needs a large conditional statement or jump table to decode each UM instruction based on its opcode and branch to the appropriate code for handling the instruction. This requires a lot of jumping around between machine instructions to decode and execute UM instructions.  
By comparison, the JIT compiler translates UM instructions into machine code each time a new program is loaded into the zero (execution) segment. After the translation is complete, the CPU can blitz through the compiled machine instructions with minimal branching.  
Most UM instructions are compiled into pure machine code, but the more complex ones (map segment, unmap segment, input, output, and load program) are compiled into machine code that branches to handwritten assembly that subsequently calls a C function. This is done because all UM instructions must get compiled to the same number of bytes of machine code to preserve alignment, so we want to avoid bloating the standard size of a compiled UM instruction.

This JIT-based UM virtual runtime also uses a custom 32 bit memory allocator to accelerate address translations for the Load Register and Store Register instructions. This memory allocator is itself an entire project; see [Virt32](https://github.com/LiamDrew/Virt32) for more information.

Thanks to these performance improvements, this JIT-based runtime executes UM programs up to 4.58 times faster than an aggressively profiled pure emulator on an Arm-based Macbook, and 3.5 times faster on an x86 server. See the performance section below for a full breakdown.

## Running the Program
1. Download the source code.  

2. Choose a platform. There are 3 root directories: linux-x86-64-container, linux-arm64-container, and darwin-arm64. As their names suggest, the linux containers run linux on Arm64 and x86-64 platforms using docker. The darwin-arm64 directory doesn't use docker, and is intended to compile natively on an Arm-based Mac running MacOS. For darwin-arm64, skip to step 6.

3. Build the docker image:  
x86-64: ```docker buildx build --platform=linux/amd64 -t dev-tools-x86 .```  
Arm64 (Aarch64): ```docker buildx build --platform=linux/arm64 -t dev-tools-aarch64 .```  
Both containers have the utilities you need to run the program. They can each access their own `docker_shared` directory, which is shared between the container and your machine.

4. Start the docker container:  
x86-64: ```docker run --platform=linux/amd64 -it -v "$PWD/docker_shared:/home/developer/shared" dev-tools-x86```  
Arm64: ```docker run --platform=linux/arm64 -it -v "$PWD/docker_shared:/home/developer/shared" dev-tools-aarch64```
5. Navigate to the ```shared``` directory

6. Each container root directory contains the following subdirectories: `/emulator`, `/jit`, `/umasm`.  
The `/umasm` directory contains a variety of Universal Machine assembly language programs.  
The `/emulator` directory contains an executable binary of a profiled emulator-based UM virtual runtime. The binary can be run with `./um ../umasm/[program.um]`. The emulator source code is intentionally omitted since the emulator is a CS40 project   
The `/jit` directory contains the executable binary source code for the JIT compiler-based UM virtual runtime.

7. Navigate to the `/jit` directory
8. Compile with `make`
9. Run a benchmark program that executes 2 million UM instructions with `./jit umasm/sandmark.umz`

## Performance

I timed my emulator and JIT-compiler runtimes on the `sandmark.umz` benchmark in 4 different environments:
1. Natively on Arm64 Darwin (MacOS)
2. An Arm64 Linux environment in a Docker container on the Mac.
3. An x86-64 Linux environment in a Docker container on the Mac, emulated by Rosetta 2 (Apple's x86-64 virtualization layer).
4. Natively on an x86-64 Linux machine on the Tufts CS department servers

| Runtime  | Architecture | Compiler  | OS     | Hardware   | Time (seconds) |
| -------- | ------------ | --------- | ------ | ---------- | -------------- |
| JIT      | Arm64        | clang -O2 | Darwin | Mac M3     | 0.53           |
| Emulator | Arm64        | clang -O2 | Darwin | Mac M3     | 2.42           |
| JIT      | Arm64        | clang -O2 | Linux  | Mac M3     | 0.61           |
| Emulator | Arm64        | clang -O2 | Linux  | Mac M3     | 2.39           |
| JIT      | x86-64*      | clang -O2 | Linux  | Mac M3     | 0.76           |
| Emulator | x86-64*      | clang -O2 | Linux  | Mac M3     | 2.73           |
| JIT      | x86-64       | gcc -O2   | Linux  | Intel Xeon | 1.44           |
| Emulator | x86-64       | gcc -O2   | Linux  | Intel Xeon | 4.45           |

*Run via emulation with Rosetta 2  
All runtimes use the [Virt32](https://github.com/LiamDrew/Virt32) memory allocator to accelerate address translations.

CPU Specs:
* M3 Max Macbook (10 Performance Cores @ 4.05 GHz & 4 Efficiency Cores @ 2.80GHz)
* Intel(R) Xeon(R) Silver 4214Y CPU (6 Cores @ 2.20GHz)

Profiling Tools:
* Apple Instruments on MacOS
* Kcachegrind on Linux

Here's what I noticed:
1. Unsuprisingly, the Mac is significantly faster than the Intel machine. Even when using Rosetta 2 as an x86 virtualization layer, the Mac ran the x86 emulator and JIT runtimes nearly 2x faster than the Intel machine running x86 natively. With the Arm version of the JIT running natively on the Mac, the gap was even wider: the Mac was 3x faster.
2. The Intel machine was *very* sensitive to the `CHUNK` size (the standard size of a compiled UM instruction), which I believe to be due to lower memory bandwidth. As a result, I aggressively cut down on chunk size, at the expense of more conditional branching in the Load Program instruction. The Mac was less sensitive to chunk size; the System-on-Chip design makes memory access blazingly fast compared to the Intel machine.
3. The Mac is about 15% slower when using Rosetta 2 as the x86 translation layer. The emulator is written entirely in C without any platform-specific assembly, and thus can be compiled on any platform. This 15% estimate is based on the ratio of runtimes for the emulator compiled to Arm assembly vs the same emulator compiled to x86 assembly and run using Rosetta.
4. The JIT ran 15% faster natively on Darwin than it did in the Arm64 linux container. I still don't have a concrete explanation for this; the only hypothesis I have is that MacOS has some funny assembly conventions (like using the `ldr` instruction to load addresses) that may require some overhead to translate from the Docker container running Linux. This performance disparity didn't appear at all between the same emulator compiled on Darwin and in the Linux container, so it's hard to say what the source of the disparity could be.

## Potential Improvements and Considerations
The benchmark Universal Machine assembly language programs used to test these runtimes have a weaknesses that I exploited to make my JIT faster. If a UM program modifies iteself by storing an instruction into the zero segment it intends to execute, the JIT compiler will have undefined behavior. To handle this case, any instruction stored in the zero segment would have to be compiled into machine code, even if it never ends up being executed. I omitted this feature because it would slow the JIT down.

This feature would be relatively easy to add in both platform versions of the JIT.

## Takeaways
This project has no practical application, but taught me a ton about computer architecture, operating systems, assembly language, and how writing performant programs.

I love working with Arm assembly & machine code. Arm has a plethora of reigsters, all instructions are encoded into 4 bytes, and three registers at a time can be used for all standard operations. This will not be my last Arm assembly project.

## Acknowledgements
Professor Mark Sheldon, for giving me the idea to make a JIT compiler for the Univeral Machine.  

Tom Hebb, for sharing a similar JIT compiler he built years ago. Tom's project had several brilliant ideas that helped me improve my original JIT compiler significantly, most notably branching from the JIT compiled machine code to hand-written assembly (written at compile time) to handle complex instructions.

Milo Goldstein, Jason Miller, Hameedah Lawal, and Yoda Ermias (my JumboHack 2025 teammates) for helping me designing and implement the [Virt32](https://github.com/LiamDrew/Virt32) memory allocator used in this project.

Peter Wolfe, my project partner for the Univeral Machine assignment. 

