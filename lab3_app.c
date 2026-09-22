#include "lab3_app.h"
#include <stdint.h>

/* =========================================================
   Arduino Due (SAM3X8E) register base addresses
   ========================================================= */
#define PMC_BASE   (0x400E0600u)
#define PIOA_BASE  (0x400E0E00u)
#define PIOC_BASE  (0x400E1200u)
#define PIOD_BASE  (0x400E1400u)

#define NVIC_ISER0   (*(volatile uint32_t*)0xE000E100u)
#define NVIC_ICPR0   (*(volatile uint32_t*)0xE000E280u)

/* Peripheral IDs (SAM3X8E): PIOA=11, PIOC=13, PIOD=14 */
#define ID_PIOA 11u
#define ID_PIOC 13u
#define ID_PIOD 14u

/* Offsets */
#define PIO_PER    0x000u
#define PIO_OER    0x010u
#define PIO_ODR    0x014u
#define PIO_IFER   0x020u
#define PIO_SODR   0x030u
#define PIO_CODR   0x034u
#define PIO_PDSR   0x03Cu
#define PIO_IER    0x040u
#define PIO_IDR    0x044u
#define PIO_ISR    0x04Cu
#define PIO_PUER   0x064u

#define PIO_AIMER  0x0B0u
#define PIO_ESR    0x0C0u
#define PIO_FELLSR 0x0D0u

#define PMC_PCER0  0x010u

#define REG32(addr) (*(volatile uint32_t*)(addr))
#define PIO_REG(base, off) REG32((base) + (off))
#define PMC_REG(off)       REG32(PMC_BASE + (off))

/* ===== Lab mapping =====
   LEDs:   PC1..PC4
   Button1 PA14 (IRQ)
   Button2 PD0  (poll)
*/
#define LED1_MASK (1u << 1)
#define LED2_MASK (1u << 2)
#define LED3_MASK (1u << 3)
#define LED4_MASK (1u << 4)

#define BTN1_MASK (1u << 14)
#define BTN2_MASK (1u << 0)

/* Global flag set by ISR (in isr_override.c), consumed by Task4 */
volatile int btn1_event = 0;
volatile int task2_waiting = 0;
static inline uint32_t led_mask_from_index(int index)
{
    return (1u << (uint32_t)index);
}

static inline int button2_pressed(void)
{
    /* pull-up => pressed when 0 */
    return ((PIO_REG(PIOD_BASE, PIO_PDSR) & BTN2_MASK) == 0u);
}

/* ================= Auxiliary functions ================= */

void setup_io(void)
{
    /* Enable clocks for PIOA, PIOC, PIOD */
    PMC_REG(PMC_PCER0) = (1u << ID_PIOA) | (1u << ID_PIOC) | (1u << ID_PIOD);

    /* LEDs output */
    PIO_REG(PIOC_BASE, PIO_PER)  = LED1_MASK | LED2_MASK | LED3_MASK | LED4_MASK;
    PIO_REG(PIOC_BASE, PIO_OER)  = LED1_MASK | LED2_MASK | LED3_MASK | LED4_MASK;
    PIO_REG(PIOC_BASE, PIO_CODR) = LED1_MASK | LED2_MASK | LED3_MASK | LED4_MASK;

    /* Button1 input + pull-up */
    PIO_REG(PIOA_BASE, PIO_PER)  = BTN1_MASK;
    PIO_REG(PIOA_BASE, PIO_ODR)  = BTN1_MASK;
    PIO_REG(PIOA_BASE, PIO_PUER) = BTN1_MASK;

    /* Button2 input + pull-up */
    PIO_REG(PIOD_BASE, PIO_PER)  = BTN2_MASK;
    PIO_REG(PIOD_BASE, PIO_ODR)  = BTN2_MASK;
    PIO_REG(PIOD_BASE, PIO_PUER) = BTN2_MASK;

    /* Configure interrupt mode for Button1 (PA14) falling edge */
    PIO_REG(PIOA_BASE, PIO_IDR) = BTN1_MASK; /* disable while configuring */
    (void)PIO_REG(PIOA_BASE, PIO_ISR);       /* clear pending */

    PIO_REG(PIOA_BASE, PIO_AIMER)  = BTN1_MASK;
    PIO_REG(PIOA_BASE, PIO_ESR)    = BTN1_MASK;
    PIO_REG(PIOA_BASE, PIO_FELLSR) = BTN1_MASK;
    PIO_REG(PIOA_BASE, PIO_IFER)   = BTN1_MASK;

    (void)PIO_REG(PIOA_BASE, PIO_ISR); /* clear again */
}

void enable_button1_irq(void)
{
    (void)PIO_REG(PIOA_BASE, PIO_ISR);     /* clear pending */
    PIO_REG(PIOA_BASE, PIO_IER) = BTN1_MASK;

    /* enable PIOA IRQ in NVIC: index 11 */
    NVIC_ICPR0 = (1u << 11);
    NVIC_ISER0 = (1u << 11);
}

/* =========================================================
   OBS: PIOA_Handler ska INTE ligga här.
   Den finns redan i isr_override.c -> annars får du duplicate definitions.
   ========================================================= */

void turn_on_led(int index)
{
    PIO_REG(PIOC_BASE, PIO_SODR) = led_mask_from_index(index);
}

void turn_off_led(int index)
{
    PIO_REG(PIOC_BASE, PIO_CODR) = led_mask_from_index(index);
}

/* ---------------------------------------------------------
   flash_led: gör en synlig blink även om LED redan är ON
   --------------------------------------------------------- */
void flash_led(int index)
{
    volatile int i, j;

    turn_off_led(index);
    for (i = 0; i < 8000; i++)
        for (j = 0; j < 100; j++)
            ;

    turn_on_led(index);
    for (i = 0; i < 8000; i++)
        for (j = 0; j < 100; j++)
            ;

    turn_off_led(index);
    for (i = 0; i < 8000; i++)
        for (j = 0; j < 100; j++)
            ;
}

void compute_primes(void)
{
    volatile long long x, y, n = 4000;
    volatile int isprime;

    for (x = 2; x < n; x++) {
        isprime = 1;
        for (y = 2; y < x; y++) {
            if ((x % y) == 0) { isprime = 0; break; }
        }
        (void)isprime;
    }
}

/* ================= Tasks ================= */

/* Task1 unchanged for Part 1 and Part 2 (per lab) */
void task_1(void)
{
    while (1) {
        turn_on_led(1);
        compute_primes();

        turn_off_led(1);
        for (int i = 0; i < 3; i++) flash_led(1);

        (void)wait(8000);
        set_deadline(task_1_deadline + ticks());
    }
}

/* Task4: poll-switch för Button2 + vidarebefordrar Button1-event till mailbox */
void task_4(void)
{
    while (1) {
        (void)wait(10);

        /* Forward Button1 event safely from task context */
        if (btn1_event) {
    btn1_event = 0;

    /* Skicka bara event om Task2 väntar just nu */
    if (task2_waiting) {
        (void)send_no_wait(input_events, NULL);
    }
}

        /* Button2 poll-switch behavior */
        if (button2_pressed()) {
            while (button2_pressed()) {
                flash_led(4);
            }
            turn_off_led(4);
        }

        set_deadline(task_4_deadline + ticks());
    }
}

#if LAB_PART == 1

void task_2(void)
{
    while (1) {
        turn_on_led(2);
        compute_primes();

        turn_off_led(2);
        for (int i = 0; i < 3; i++) flash_led(2);

        (void)wait(8000);
        set_deadline(task_2_deadline + ticks());
    }
}

void task_3(void)
{
    while (1) {
        turn_on_led(3);
        compute_primes();

        turn_off_led(3);
        for (int i = 0; i < 3; i++) flash_led(3);

        (void)wait(8000);
        set_deadline(task_3_deadline + ticks());
    }
}

#elif LAB_PART == 2

void task_2(void)
{
    while (1) {
        /* Deadline för receive_wait-timeout */
        set_deadline(task_2_deadline + ticks());

      turn_on_led(2);

bool pressed = 1;

/* HÄR: markera att Task2 faktiskt väntar */
task2_waiting = 1;
exception ex = receive_wait(input_events, &pressed);
task2_waiting = 0;   /* HÄR: inte längre väntande */

        if (ex == OK) {
            turn_off_led(2);
            for (int i = 0; i < 3; i++) flash_led(2);

            uint32_t msg = 1;
            (void)send_no_wait(t2_to_t3, &msg);
        } else {
            /* Timeout/fail */
            turn_off_led(2);
        }

        /* VIKTIGT: flytta deadline framåt så wait(8000) inte avbryts */
        set_deadline(task_2_deadline + ticks());

        (void)wait(8000);
    }
}

void task_3(void)
{
    while (1) {
        set_deadline(task_3_deadline + ticks());

        uint32_t msg = 0;
        exception ex = receive_wait(t2_to_t3, &msg);

        if (ex == OK) {
            turn_on_led(3);
            compute_primes();

            for (int i = 0; i < 3; i++) flash_led(2);

            turn_off_led(3);

            /* flytta deadline framåt innan wait */
            set_deadline(task_3_deadline + ticks());
            (void)wait(8000);
        } else {
            turn_off_led(3);
        }
    }
}

#else
#error "LAB_PART must be 1 or 2"
#endif