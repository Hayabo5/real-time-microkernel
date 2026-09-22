#ifndef KERNEL_FUNCTIONS_H
#define KERNEL_FUNCTIONS_H

#include <stdlib.h>
#include <string.h>
#include <limits.h>

#define CONTEXT_SIZE    8
#define STACK_SIZE      100

#define TRUE    1
#define FALSE   (!TRUE)

#define RUNNING 1
#define INIT    (!RUNNING)

/* return codes */
#define FAIL             0
#define OK               1
#define DEADLINE_REACHED 2
#define NOT_EMPTY        3

#define SUCCESS  1

typedef int             exception;
typedef int             bool;
typedef unsigned int    uint;

/* forward declarations */
typedef struct l_obj   listobj;
typedef struct _list   list;
typedef struct mailbox mailbox;

/* ================= TCB (layout MUST match asm) ================= */
typedef struct
{
    uint    *SP;                        /* asm stores PSP here */
    uint    R4toR11[CONTEXT_SIZE];      /* asm stores r4-r11 here */
    void    (*PC)();
    uint    SPSR;
    uint    StackSeg[STACK_SIZE];
    uint    Deadline;
} TCB;

/* ================= Message ================= */
typedef struct msgobj
{
    char            *pData;      /* points to sender/receiver buffer OR allocated data copy */
    exception        Status;     /* OK / DEADLINE_REACHED / internal blocked codes */
    listobj         *pBlock;     /* blocked task (listobj) or NULL for data messages */
    mailbox         *pOwner;     /* owning mailbox (for cleanup on deadline) */
    struct msgobj   *pPrevious;
    struct msgobj   *pNext;
} msg;

/* ================= Mailbox ================= */
struct mailbox
{
    msg     *pHead;
    msg     *pTail;
    int      nDataSize;
    int      nMaxMessages;
    int      nMessages;
    int      nBlockedMsg;
};

/* ================= Generic list ================= */
struct l_obj
{
    TCB            *pTask;
    uint            nTCnt;        /* used by TimerList: wake tick */
    msg            *pMessage;     /* blocked/wait status message */
    struct l_obj   *pPrevious;
    struct l_obj   *pNext;
};

struct _list
{
    listobj *pHead;
    listobj *pTail;
};

/* globals (defined in kernel_functions.c) */
extern int   Ticks;
extern int   KernelMode;
extern TCB  *PreviousTask, *NextTask;
extern list *ReadyList, *WaitingList, *TimerList;

/* Lab 1 */
exception   init_kernel(void);
exception   create_task(void (*task_body)(), uint deadline);
void        terminate(void);
void        run(void);

/* Lab 2 IPC */
mailbox*    create_mailbox(uint nMessages, uint nDataSize);
exception   remove_mailbox(mailbox* mBox);
int         no_messages(mailbox* mBox);

exception   send_wait(mailbox* mBox, void* pData);
exception   receive_wait(mailbox* mBox, void* pData);
/* NOTE (Lab 3 support):
   send_no_wait(mBox, NULL) means "send zero" => receiver gets nDataSize bytes of 0. */
exception   send_no_wait(mailbox* mBox, void* pData);
exception   receive_no_wait(mailbox* mBox, void* pData);

/* Lab 2 Timing */
exception   wait(uint nTicks);
void        set_ticks(uint nTicks);
uint        ticks(void);
uint        deadline(void);
void        set_deadline(uint deadline);

/* MUST be called from asm SysTick_Handler */
void        TimerInt(void);

/* asm functions (provided by teacher) */
extern void isr_off(void);
extern void isr_on(void);

extern void SwitchContext(void);
extern void LoadContext_In_Run(void);
extern void switch_to_stack_of_next_task(void);
extern void LoadContext_In_Terminate(void);

#endif
