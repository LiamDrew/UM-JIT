# UMJIT

A just-in-time compiler from Universal Machine assembly language to x86 assembly language.

## Overview

The Universal Machine (or UM) is an simple virtual machine that all CS students at Tufts implement
in CS40: Machine Structure and Assembly Language Programming. The UM has 8 32-bit registers, recognizes 14 instructions, and has 32-bit word-oriented memory. One special memory segment contains the current UM "program" that is being executed. The memory of the UM is bounded only by the memory constraints of the host machine.

Students implement the UM as an emulator, typically with a stack-allocated array of registers and
heap allocated memory segments. This implementation just-in-time compiles UM instructions to x86 machine code and executes it inline. The program result is exactly the same, with a very different implementation.

## Why x86?
I chose x86 for two reasons. First, I wanted to be able to run my program on the Tufts CS department servers, which run x86_64 linux. Second, I was already familiar with x86 assembly syntax, so writing the assembly instructions as psuedocode for the machine code made a lot of sense.

It's worth noting that the UM instruction set is so simple that the machine code it compiles to is simple as well. All the x86 instructions that get executed are simple movs, adds, mults, divs, ands, nots, ect.

In the future, if I succeed in optimizing this compiler to run faster on the x86 hardware, I will make a UM to ARM compiler in the future. I developed this compiler in a docker container emulating x86_64 Linux on an ARM Mac, so I'm curious to see just how fast this could be done on the native hardware. Because the machine instructions this program compiles to are not complicated, I'm optimistic that porting the compiler to an ARM system will be pretty straightforward.

## Design

## Challenges
This program is not portable, as it executes x86 machine code.

## Potential Improvements

## Acknowledgements
Thank you to Professor Mark Sheldon who gave me the idea for this project.
Thank you to Professor Norman Ramsey who created the (excellent) UM assignment for CS40.


 
