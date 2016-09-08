# TLB Analyzer

![framework](https://github.com/whitestonelee/tlb_analyzer/blob/master/img/framework.png)

## Introduction
TLB Analyzer is a library for analyzing cache usage for virtualization environments.
It consists of two modules TLB Tracer and TLB Simulator.

TLB Tracer provides a framework for tracing memory accesses.
It can be easily integrated into other applications.
This project comtains an example of integrating with Android Emulator.
It also defines an open file format for storing traces of memory accesses.
For more details, please refer to tlb_trace.h.

TLB Simulator provides a framework for analyzing cache usage based on trace files.
It can simulate several common types of nested paging, including using NTLB, PWC, and both.
It accepts traces of memory accesses stored in the format defined by TLB Tracer.
It also provides a helper function that can run simulation with all trace files in a specific folder.
for more details, please refer to tlb_sim.h.

## Usage
First, type 'make' in a commad line to build this project.

To use this library, please link to libtlb_analyzer.a statically.

To run the example for TLB Simulator, please execute 'tlb_sim' in a command line.

An example of integrating TLB Tracer with Android Emulator is located at the folder 'qemu'.
