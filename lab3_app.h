#ifndef LAB3_APP_H
#define LAB3_APP_H

#include <stdint.h>
#include "kernel_functions.h"

/* Part selector */
#ifndef LAB_PART
#define LAB_PART 2
#endif

extern uint task_1_deadline;
extern uint task_2_deadline;
extern uint task_3_deadline;
extern uint task_4_deadline;

/* Mailboxes */
extern mailbox *input_events;
extern mailbox *t2_to_t3;

extern volatile int task2_waiting; ////////////////////////////////////////////////////////////////////////////


/* Button1 event flag (set in ISR, consumed in Task4) */
extern volatile int btn1_event;
extern volatile uint task2_wait_start;
extern volatile int  task2_waiting;
void setup_io(void);
void enable_button1_irq(void);

void turn_on_led(int index);
void turn_off_led(int index);
void flash_led(int index);
void compute_primes(void);

void task_1(void);
void task_2(void);
void task_3(void);
void task_4(void);

/* Interrupt handler from startup vector */
void PIOA_Handler(void);

#endif