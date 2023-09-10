# Mini Operating System

Group project by Mengwei Yuan, Kexin Chen, Moyu Li, Yanbo Li

- Built in C and tested on standard 32-bit Ubuntu 16.04 Linux system
- Based on Weenix operating system developed by Brown University
- The kernel file contains all code we developed
- In proc file, implemented process and thread control with context switching, mutex lock, and terminating mechanism.
- In fs file, developed file system with path parsing, file object creation, permission checking, and read/write operation.
- In vm file, built virtual memory with page frame and virtual address mapping, shadow object and reference counting.