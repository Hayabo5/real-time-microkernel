# Real-Time Microkernel (RTMK)

## About the Project

This project is a real-time microkernel implemented in C for the SAM3X8E ARM microcontroller using IAR Embedded Workbench.

The goal of the project was to develop core operating system functionality for an embedded system, including task management, inter-process communication, scheduling, timing and hardware interaction.

## Features

- Task creation and termination
- Deadline-based task scheduling
- Context switching between tasks
- Ready, waiting and timer task lists
- Inter-process communication using mailboxes
- Blocking and non-blocking message handling
- Task timing and deadline management
- SysTick-based system timing
- GPIO control for LEDs and push buttons
- Hardware interrupt handling

## Implementation

The microkernel manages multiple tasks using Task Control Blocks (TCBs) and linked lists. Tasks are scheduled according to their deadlines and can communicate through mailbox-based message passing.

The application also demonstrates the kernel on an Arduino Due / SAM3X8E platform using multiple concurrent tasks, LEDs, push buttons and hardware interrupts.

## Technologies

- C
- Embedded Systems
- Real-Time Systems
- ARM / SAM3X8E
- IAR Embedded Workbench
- Task Scheduling
- Inter-Process Communication
- Interrupts and SysTick

## Source Code

- `kernel_functions.c` – Core microkernel, scheduling, mailboxes and timing
- `kernel_functions.h` – Kernel data structures and function definitions
- `lab3_app.c` – Tasks and hardware interaction
- `lab3_app.h` – Application definitions
- `main.c` – System initialization and task creation

## Academic Project

Developed as part of the Computer Systems Engineering II course at Halmstad University.
