# cpu-scheduling

## Overview
This repository contains a program written in C for simulating CPU scheduling and execution using different scheduling algorithms and processors. The program takes a binary file containing Process Control Block (PCB) information about multiple processes and simulates their execution on an imaginary computer. Each processor runs independently with a specific scheduling algorithm.

## Instructions
To run the program, follow these steps:
1. Compile the source code using a C++ compiler:
   ```
   g++ main.cpp  -o scheduler
2. Run the program with the following command-line arguments:
     ```
     ./scheduler A.bin <algorithm_1> <load_1> <algorithm_2> <load_2> ...
     ```
3. For example, to simulate CPU scheduling with three processors:
     ```
     ./scheduler A.bin 1 0.4 2 0.5 3 0.1
     ```
