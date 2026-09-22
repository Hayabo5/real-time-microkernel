#include "kernel_functions.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* ===================== globals ===================== */
int   Ticks = 0;
int   KernelMode = INIT;
TCB  *PreviousTask = NULL;
TCB  *NextTask = NULL;
list *ReadyList = NULL;
list *WaitingList = NULL;
list *TimerList = NULL;

/* ===================== internal codes ===================== */
#define BLOCKED_SENDER    1001
#define BLOCKED_RECEIVER  1002
#define WAIT_STATUS_MSG   1003

/* ===================== ISR detection ===================== */
/* IPSR != 0 => running in exception/interrupt context */
static inline int in_isr(void)
{
#if defined(__ICCARM__)
  #include <intrinsics.h>
    return (__get_IPSR() != 0);
#else
    uint32_t ipsr;
    __asm volatile ("MRS %0, IPSR" : "=r"(ipsr));
    return (ipsr != 0);
#endif
}

/* ===================== list helpers ===================== */
static list* list_create(void)
{
    return (list*)calloc(1, sizeof(list));
}

static listobj* listobj_create_task(TCB *t)
{
    listobj *o = (listobj*)calloc(1, sizeof(listobj));
    if (!o) return NULL;
    o->pTask = t;
    o->nTCnt = 0;
    o->pMessage = NULL;
    o->pPrevious = NULL;
    o->pNext = NULL;
    return o;
}

static void list_append(list *L, listobj *obj)
{
    obj->pNext = NULL;
    obj->pPrevious = L->pTail;
    if (L->pTail) L->pTail->pNext = obj;
    else L->pHead = obj;
    L->pTail = obj;
}

static void list_remove_node(list *L, listobj *obj)
{
    if (obj->pPrevious) obj->pPrevious->pNext = obj->pNext;
    else L->pHead = obj->pNext;

    if (obj->pNext) obj->pNext->pPrevious = obj->pPrevious;
    else L->pTail = obj->pPrevious;

    obj->pNext = NULL;
    obj->pPrevious = NULL;
}

static listobj* list_extract_head(list *L)
{
    if (!L || !L->pHead) return NULL;
    listobj *out = L->pHead;
    L->pHead = out->pNext;
    if (L->pHead) L->pHead->pPrevious = NULL;
    else L->pTail = NULL;
    out->pNext = out->pPrevious = NULL;
    return out;
}

/* ReadyList sorted by Deadline (earliest first) */
static void readylist_insert_sorted(list *L, listobj *obj)
{
    if (!L->pHead) {
        L->pHead = L->pTail = obj;
        obj->pNext = obj->pPrevious = NULL;
        return;
    }

    listobj *cur = L->pHead;
    while (cur) {
        if (obj->pTask->Deadline < cur->pTask->Deadline) {
            obj->pNext = cur;
            obj->pPrevious = cur->pPrevious;
            if (cur->pPrevious) cur->pPrevious->pNext = obj;
            else L->pHead = obj;
            cur->pPrevious = obj;
            return;
        }
        cur = cur->pNext;
    }

    obj->pPrevious = L->pTail;
    obj->pNext = NULL;
    L->pTail->pNext = obj;
    L->pTail = obj;
}

/* TimerList sorted by wake tick (nTCnt) */
static void timerlist_insert_sorted(list *L, listobj *obj)
{
    if (!L->pHead) {
        L->pHead = L->pTail = obj;
        obj->pNext = obj->pPrevious = NULL;
        return;
    }

    listobj *cur = L->pHead;
    while (cur) {
        if (obj->nTCnt < cur->nTCnt) {
            obj->pNext = cur;
            obj->pPrevious = cur->pPrevious;
            if (cur->pPrevious) cur->pPrevious->pNext = obj;
            else L->pHead = obj;
            cur->pPrevious = obj;
            return;
        }
        cur = cur->pNext;
    }

    obj->pPrevious = L->pTail;
    obj->pNext = NULL;
    L->pTail->pNext = obj;
    L->pTail = obj;
}

/* ===================== mailbox helpers ===================== */
static void mailbox_append(mailbox *mb, msg *m)
{
    m->pNext = NULL;
    m->pPrevious = mb->pTail;
    if (mb->pTail) mb->pTail->pNext = m;
    else mb->pHead = m;
    mb->pTail = m;
    mb->nMessages++;
}

static void mailbox_remove_exact(mailbox *mb, msg *m)
{
    if (m->pPrevious) m->pPrevious->pNext = m->pNext;
    else mb->pHead = m->pNext;

    if (m->pNext) m->pNext->pPrevious = m->pPrevious;
    else mb->pTail = m->pPrevious;

    m->pNext = m->pPrevious = NULL;
    mb->nMessages--;
}

static msg* mailbox_find_first_status(mailbox *mb, int status)
{
    msg *cur = mb->pHead;
    while (cur) {
        if (cur->Status == status) return cur;
        cur = cur->pNext;
    }
    return NULL;
}

/* first FIFO data message = pBlock == NULL */
static msg* mailbox_pop_first_data(mailbox *mb)
{
    msg *cur = mb->pHead;
    while (cur) {
        if (cur->pBlock == NULL) {
            mailbox_remove_exact(mb, cur);
            return cur;
        }
        cur = cur->pNext;
    }
    return NULL;
}

/* helper: copy pData to dst, but if pData==NULL => write zeros */
static void copy_or_zero(void *dst, const void *pData, size_t n)
{
    if (pData) memcpy(dst, pData, n);
    else memset(dst, 0, n);
}

/* ===================== idle ===================== */
static void idle_task(void)
{
    while (1) { 
   
    
    }
}

/* ===================== Lab 1 ===================== */
exception init_kernel(void)
{
    Ticks = 0;
    KernelMode = INIT;
    PreviousTask = NULL;
    NextTask = NULL;

    ReadyList   = list_create();
    WaitingList = list_create();
    TimerList   = list_create();
    if (!ReadyList || !WaitingList || !TimerList) return FAIL;

    /* idle task always last */
    if (create_task(idle_task, UINT_MAX) != OK) return FAIL;

    return OK;
}

exception create_task(void (*task_body)(), uint deadline)
{
    TCB *t = (TCB*)calloc(1, sizeof(TCB));
    if (!t) return FAIL;

    t->PC       = task_body;
    t->SPSR     = 0x21000000;
    t->Deadline = deadline;

    /* stack frame expected by teacher asm */
    t->StackSeg[STACK_SIZE - 2] = 0x21000000;      /* xPSR */
    t->StackSeg[STACK_SIZE - 3] = (uint)task_body; /* PC   */
    t->SP = &(t->StackSeg[STACK_SIZE - 9]);        /* points to r0 position */

    listobj *o = listobj_create_task(t);
    if (!o) { free(t); return FAIL; }

    if (KernelMode == INIT) {
        readylist_insert_sorted(ReadyList, o);
        return OK;
    }

    /* RUNNING: preempt if new head */
    isr_off();

    listobj *runningObj = ReadyList->pHead;
    PreviousTask = runningObj->pTask;

    readylist_insert_sorted(ReadyList, o);

    NextTask = ReadyList->pHead->pTask;

    if (ReadyList->pHead != runningObj) {
        SwitchContext();
        return OK;
    }

    isr_on();
    return OK;
}

void run(void)
{
    Ticks = 0;
    KernelMode = RUNNING;

    NextTask = ReadyList->pHead->pTask;
    LoadContext_In_Run();

    while (1) { }
}

void terminate(void)
{
    isr_off();

    listobj *leaving = list_extract_head(ReadyList);
    if (!leaving) while (1) { }

    NextTask = ReadyList->pHead->pTask;

    switch_to_stack_of_next_task();

    free(leaving->pTask);
    free(leaving);

    LoadContext_In_Terminate();

    while (1) { }
}

/* ===================== Lab 2: Mailboxes ===================== */
mailbox* create_mailbox(uint nMessages, uint nDataSize)
{
    mailbox *mb = (mailbox*)calloc(1, sizeof(mailbox));
    if (!mb) return NULL;
    mb->pHead = mb->pTail = NULL;
    mb->nDataSize = (int)nDataSize;
    mb->nMaxMessages = (int)nMessages;
    mb->nMessages = 0;
    mb->nBlockedMsg = 0;
    return mb;
}

exception remove_mailbox(mailbox* mBox)
{
    if (!mBox) return FAIL;
    if (mBox->nMessages == 0 && mBox->pHead == NULL) {
        free(mBox);
        return OK;
    }
    return NOT_EMPTY;
}

int no_messages(mailbox* mBox)
{
    if (!mBox) return 0;
    return mBox->nMessages;
}

/* === send_no_wait: safe both in task context and ISR context ===
   IMPORTANT: In ISR, this function will NEVER call SwitchContext().
*/
exception send_no_wait(mailbox* mBox, void* pData)
{
    if (!mBox) return FAIL;

    int is_isr = in_isr();

    if (!is_isr) isr_off();

    listobj *runningObj = ReadyList->pHead;

    /* receiver waiting? */
    msg *r = mailbox_find_first_status(mBox, BLOCKED_RECEIVER);
    if (r) {
        copy_or_zero(r->pData, pData, (size_t)mBox->nDataSize);

        mailbox_remove_exact(mBox, r);
        mBox->nBlockedMsg--;

        r->Status = OK;
        listobj *recvObj = r->pBlock;

        list_remove_node(WaitingList, recvObj);
        readylist_insert_sorted(ReadyList, recvObj);

        NextTask = ReadyList->pHead->pTask;
        PreviousTask = runningObj->pTask;

        /* Task context: preempt if needed */
        if (!is_isr && ReadyList->pHead != runningObj) {
            SwitchContext();
            return OK;
        }

        if (!is_isr) isr_on();
        return OK;
    }

    /* store data message (copy) */
    msg *m = (msg*)calloc(1, sizeof(msg));
    if (!m) { if (!is_isr) isr_on(); return FAIL; }

    m->pOwner = mBox;
    m->pBlock = NULL;
    m->Status = OK;
    m->pData = (char*)calloc(1, (size_t)mBox->nDataSize);
    if (!m->pData) { free(m); if (!is_isr) isr_on(); return FAIL; }

    copy_or_zero(m->pData, pData, (size_t)mBox->nDataSize);

    if (mBox->nMessages >= mBox->nMaxMessages) {
        msg *old = mailbox_pop_first_data(mBox);
        if (old) {
            if (old->pData) free(old->pData);
            free(old);
        }
    }

    mailbox_append(mBox, m);

    if (!is_isr) isr_on();
    return OK;
}

exception receive_no_wait(mailbox* mBox, void* pData)
{
    if (!mBox || !pData) return FAIL;

    isr_off();

    listobj *runningObj = ReadyList->pHead;

    msg *d = mailbox_pop_first_data(mBox);
    if (d) {
        memcpy(pData, d->pData, (size_t)mBox->nDataSize);
        free(d->pData);
        free(d);
        isr_on();
        return OK;
    }

    msg *s = mailbox_find_first_status(mBox, BLOCKED_SENDER);
    if (s) {
        memcpy(pData, s->pData, (size_t)mBox->nDataSize);

        mailbox_remove_exact(mBox, s);
        mBox->nBlockedMsg--;

        s->Status = OK;
        listobj *sendObj = s->pBlock;

        list_remove_node(WaitingList, sendObj);
        readylist_insert_sorted(ReadyList, sendObj);

        NextTask = ReadyList->pHead->pTask;
        PreviousTask = runningObj->pTask;

        if (ReadyList->pHead != runningObj) {
            SwitchContext();
            return OK;
        }

        isr_on();
        return OK;
    }

    isr_on();
    return FAIL;
}

exception send_wait(mailbox* mBox, void* pData)
{
    if (!mBox || !pData) return FAIL;

    isr_off();

    listobj *runningObj = ReadyList->pHead;

    msg *r = mailbox_find_first_status(mBox, BLOCKED_RECEIVER);
    if (r) {
        memcpy(r->pData, pData, (size_t)mBox->nDataSize);

        mailbox_remove_exact(mBox, r);
        mBox->nBlockedMsg--;

        r->Status = OK;
        listobj *recvObj = r->pBlock;

        list_remove_node(WaitingList, recvObj);
        readylist_insert_sorted(ReadyList, recvObj);

        NextTask = ReadyList->pHead->pTask;
        PreviousTask = runningObj->pTask;

        if (ReadyList->pHead != runningObj) SwitchContext();
        else isr_on();

        return OK;
    }

    msg *m = (msg*)calloc(1, sizeof(msg));
    if (!m) { isr_on(); return FAIL; }

    m->pOwner = mBox;
    m->pData  = (char*)pData;     /* sender buffer */
    m->pBlock = runningObj;
    m->Status = BLOCKED_SENDER;

    mailbox_append(mBox, m);
    mBox->nBlockedMsg++;

    runningObj->pMessage = m;

    list_remove_node(ReadyList, runningObj);
    list_append(WaitingList, runningObj);

    PreviousTask = runningObj->pTask;
    NextTask = ReadyList->pHead->pTask;

    SwitchContext();

    exception st = runningObj->pMessage->Status;
    free(runningObj->pMessage);
    runningObj->pMessage = NULL;

    return st;
}

exception receive_wait(mailbox* mBox, void* pData)
{
    if (!mBox || !pData) return FAIL;

    isr_off();

    listobj *runningObj = ReadyList->pHead;

    msg *d = mailbox_pop_first_data(mBox);
    if (d) {
        memcpy(pData, d->pData, (size_t)mBox->nDataSize);
        free(d->pData);
        free(d);
        isr_on();
        return OK;
    }

    msg *s = mailbox_find_first_status(mBox, BLOCKED_SENDER);
    if (s) {
        memcpy(pData, s->pData, (size_t)mBox->nDataSize);

        mailbox_remove_exact(mBox, s);
        mBox->nBlockedMsg--;

        s->Status = OK;
        listobj *sendObj = s->pBlock;

        list_remove_node(WaitingList, sendObj);
        readylist_insert_sorted(ReadyList, sendObj);

        NextTask = ReadyList->pHead->pTask;
        PreviousTask = runningObj->pTask;

        if (ReadyList->pHead != runningObj) SwitchContext();
        else isr_on();

        return OK;
    }

    msg *m = (msg*)calloc(1, sizeof(msg));
    if (!m) { isr_on(); return FAIL; }

    m->pOwner = mBox;
    m->pData  = (char*)pData;     /* receiver buffer */
    m->pBlock = runningObj;
    m->Status = BLOCKED_RECEIVER;

    mailbox_append(mBox, m);
    mBox->nBlockedMsg++;

    runningObj->pMessage = m;

    list_remove_node(ReadyList, runningObj);
    list_append(WaitingList, runningObj);

    PreviousTask = runningObj->pTask;
    NextTask = ReadyList->pHead->pTask;

    SwitchContext();

    exception st = runningObj->pMessage->Status;
    free(runningObj->pMessage);
    runningObj->pMessage = NULL;

    return st;
}

/* ===================== Lab 2: Timing ===================== */
exception wait(uint nTicks)
{
    isr_off();

    listobj *runningObj = ReadyList->pHead;

    msg *w = (msg*)calloc(1, sizeof(msg));
    if (!w) { isr_on(); return FAIL; }
    w->pOwner = NULL;
    w->pData = NULL;
    w->pBlock = runningObj;
    w->Status = WAIT_STATUS_MSG;

    runningObj->pMessage = w;
    runningObj->nTCnt = (uint)Ticks + nTicks;

    list_remove_node(ReadyList, runningObj);
    timerlist_insert_sorted(TimerList, runningObj);

    PreviousTask = runningObj->pTask;
    NextTask = ReadyList->pHead->pTask;

    SwitchContext();

    exception st = runningObj->pMessage->Status;
    free(runningObj->pMessage);
    runningObj->pMessage = NULL;

    return st;
}

void set_ticks(uint nTicks)
{
    isr_off();
    Ticks = (int)nTicks;
    isr_on();
}

uint ticks(void)
{
    return (uint)Ticks;
}

uint deadline(void)
{
    return (uint)ReadyList->pHead->pTask->Deadline;
}

void set_deadline(uint dl)
{
    isr_off();

    listobj *runningObj = ReadyList->pHead;
    runningObj->pTask->Deadline = dl;

    list_remove_node(ReadyList, runningObj);
    readylist_insert_sorted(ReadyList, runningObj);

    PreviousTask = runningObj->pTask;
    NextTask = ReadyList->pHead->pTask;

    if (ReadyList->pHead != runningObj) SwitchContext();
    else isr_on();
}

/* ===================== TimerInt (called from asm SysTick_Handler) ===================== */
void TimerInt(void)
{
    if (KernelMode != RUNNING) {
        Ticks++;
        return;
    }

    Ticks++;

    listobj *runningObj = ReadyList->pHead;

    /* wake from TimerList */
    listobj *cur = TimerList->pHead;
    while (cur) {
        listobj *next = cur->pNext;

        int wakeDone = ((uint)Ticks >= (uint)cur->nTCnt);
        int dlHit    = ((uint)Ticks >= (uint)cur->pTask->Deadline);

        if (wakeDone || dlHit) {
            list_remove_node(TimerList, cur);

            if (cur->pMessage && cur->pMessage->Status == WAIT_STATUS_MSG) {
                cur->pMessage->Status = dlHit ? DEADLINE_REACHED : OK;
            }

            readylist_insert_sorted(ReadyList, cur);
        }

        cur = next;
    }

    /* deadlines for blocked send/receive */
    cur = WaitingList->pHead;
    while (cur) {
        listobj *next = cur->pNext;

        if ((uint)Ticks >= (uint)cur->pTask->Deadline) {
            if (cur->pMessage) {
                msg *m = cur->pMessage;

                if (m->pOwner) {
                    mailbox_remove_exact(m->pOwner, m);
                    m->pOwner->nBlockedMsg--;
                }

                m->Status = DEADLINE_REACHED;
            }

            list_remove_node(WaitingList, cur);
            readylist_insert_sorted(ReadyList, cur);
        }

        cur = next;
    }

    NextTask = ReadyList->pHead->pTask;
    PreviousTask = runningObj->pTask;
    
     
}