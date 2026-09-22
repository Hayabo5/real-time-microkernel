#include "system_sam3x.h"
#include "at91sam3x8.h"
#include "kernel_functions.h"
#include "lab3_app.h"
#include <stdint.h>

/* Mailboxes */
mailbox *input_events = 0;
mailbox *t2_to_t3     = 0;

/* Deadlines (default: Part 1 / “normal”) */
uint task_1_deadline = 10000;
uint task_2_deadline = 10000;
uint task_3_deadline = 10000;
uint task_4_deadline = 10000;

int main(void)
{
    SystemInit();

    /* lab requirement */
    SysTick_Config(83999);
    SCB->SHP[((uint32_t)(SysTick_IRQn) & 0xFu) - 4u] = 0xE0;

    isr_off();

    if (init_kernel() != OK) {
        while (1) {}
    }
                                                            
    setup_io();
    enable_button1_irq();

    /* ==========================================
       Part 2 DEBUG ONLY: make timeout quicker
       (does NOT affect Part 1)
       ========================================== */


    /* Mailboxes (Part 2 needs them) */
    input_events = create_mailbox(1, sizeof(bool));
    t2_to_t3     = create_mailbox(1, sizeof(uint32_t));
    if (!input_events || !t2_to_t3) {
        while (1) {}
    }

    if (create_task(task_1, task_1_deadline) != OK) while (1) {}
    if (create_task(task_2, task_2_deadline) != OK) while (1) {}
    if (create_task(task_3, task_3_deadline) != OK) while (1) {}
    if (create_task(task_4, task_4_deadline) != OK) while (1) {}

    isr_on();
    run();

    while (1) {}
}