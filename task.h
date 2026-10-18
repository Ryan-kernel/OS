#ifndef TASK_H
#define TASK_H

typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef unsigned long  uint64_t;

#define TASK_STACK_SIZE  8192
#define MAX_TASKS        16

#define TASK_STATE_DEAD      0
#define TASK_STATE_READY     1
#define TASK_STATE_SLEEPING  2

struct task {
    uint64_t rsp;
    uint64_t stack_base;
    uint64_t stack_top;
    uint64_t id;
    char     name[32];
    int      state;
    uint64_t sleep_until;
};

void          task_init(void);
int           task_create(const char *name, void (*entry)(void));
void          task_yield(void);
void          task_exit(void);
void          task_sleep(uint32_t seconds);
uint64_t      schedule(uint64_t current_rsp);
void          task_list(void);

#endif