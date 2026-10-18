#include "task.h"

extern void kprintf(const char *fmt, ...);
extern void *kmalloc(uint64_t size);
extern uint64_t pit_seconds(void);

static struct task g_tasks[MAX_TASKS];
static int         g_task_count = 0;
static int         g_current_idx = 0;

/* ---------------- 初始化 ---------------- */

void task_init(void) {
    g_task_count = 0;
    g_current_idx = 0;
    for (int i = 0; i < MAX_TASKS; i++) {
        g_tasks[i].rsp = 0;
        g_tasks[i].stack_base = 0;
        g_tasks[i].stack_top = 0;
        g_tasks[i].id = 0;
        g_tasks[i].name[0] = 0;
        g_tasks[i].state = TASK_STATE_DEAD;
        g_tasks[i].sleep_until = 0;
    }
}

/* ---------------- 创建任务 ---------------- */

int task_create(const char *name, void (*entry)(void)) {
    if (g_task_count >= MAX_TASKS) {
        kprintf("[task] too many tasks\n");
        return -1;
    }

    struct task *t = &g_tasks[g_task_count];

    void *stack = kmalloc(TASK_STACK_SIZE);
    if (!stack) {
        kprintf("[task] out of memory\n");
        return -1;
    }

    t->stack_base = (uint64_t)stack;
    t->stack_top  = (uint64_t)stack + TASK_STACK_SIZE;
    t->id         = (uint64_t)g_task_count + 1;
    t->state      = TASK_STATE_READY;
    t->sleep_until = 0;

    int i = 0;
    while (name[i] && i < 31) { t->name[i] = name[i]; i++; }
    t->name[i] = 0;

    /* 构造初始栈 */
    uint64_t *sp = (uint64_t *)t->stack_top;

    *(--sp) = 0x10;
    *(--sp) = t->stack_top;
    *(--sp) = 0x202;
    *(--sp) = 0x18;
    *(--sp) = (uint64_t)entry;

    *(--sp) = 0;   /* r11 */
    *(--sp) = 0;   /* r10 */
    *(--sp) = 0;   /* r9  */
    *(--sp) = 0;   /* r8  */
    *(--sp) = 0;   /* rdi */
    *(--sp) = 0;   /* rsi */
    *(--sp) = 0;   /* rdx */
    *(--sp) = 0;   /* rcx */
    *(--sp) = 0;   /* rax */

    t->rsp = (uint64_t)sp;

    kprintf("[task] created '%s' (id=%u)\n", name, (unsigned int)t->id);
    g_task_count++;
    return (int)t->id;
}

/* ---------------- 调度器 ---------------- */

uint64_t schedule(uint64_t current_rsp) {
    if (g_task_count == 0) return current_rsp;

    /* 保存当前任务的 RSP（如果当前任务还活着） */
    if (g_tasks[g_current_idx].state != TASK_STATE_DEAD) {
        g_tasks[g_current_idx].rsp = current_rsp;
    }

    /* 先处理 SLEEPING 任务的唤醒 */
    uint64_t now = pit_seconds();
    for (int i = 0; i < g_task_count; i++) {
        if (g_tasks[i].state == TASK_STATE_SLEEPING) {
            if (now >= g_tasks[i].sleep_until) {
                g_tasks[i].state = TASK_STATE_READY;
            }
        }
    }

    /* 找下一个就绪任务 */
    for (int i = 1; i <= g_task_count; i++) {
        int idx = (g_current_idx + i) % g_task_count;
        if (g_tasks[idx].state == TASK_STATE_READY) {
            g_current_idx = idx;
            return g_tasks[idx].rsp;
        }
    }

    /* 没有别的任务可跑 */
    return current_rsp;
}

/* ---------------- 主动让出 ---------------- */

void task_yield(void) {
    __asm__ volatile ("int $0x20");
}

/* ---------------- 任务退出 ---------------- */

void task_exit(void) {
    /* 把当前任务标 DEAD */
    g_tasks[g_current_idx].state = TASK_STATE_DEAD;

    /* 触发调度切换到别的任务
     * 因为当前任务的 RSP 不会再被用，直接让调度器挑别的 */
    __asm__ volatile ("int $0x20");

    /* 正常情况下不会返回到这里。如果只剩当前任务，会死循环 */
    while (1) __asm__ volatile ("hlt");
}

/* ---------------- 任务睡眠 ---------------- */

void task_sleep(uint32_t seconds) {
    if (seconds == 0) { task_yield(); return; }
    uint64_t target = pit_seconds() + seconds;
    g_tasks[g_current_idx].sleep_until = target;
    
    while (pit_seconds() < target) {
        g_tasks[g_current_idx].state = TASK_STATE_SLEEPING;
        __asm__ volatile ("sti; hlt"); // 开中断并等待硬件 PIT 唤醒
    }
    g_tasks[g_current_idx].state = TASK_STATE_READY;
}

/* ---------------- 列出任务 ---------------- */

void task_list(void) {
    kprintf("Tasks (%d):\n", g_task_count);
    for (int i = 0; i < g_task_count; i++) {
        struct task *t = &g_tasks[i];
        const char *mark = (i == g_current_idx) ? " *" : "";
        const char *state_s = "?";
        if (t->state == TASK_STATE_DEAD)     state_s = "DEAD";
        if (t->state == TASK_STATE_READY)    state_s = "READY";
        if (t->state == TASK_STATE_SLEEPING) state_s = "SLEEP";

        kprintf("  #%-3u %-16s %-6s rsp=%p%s\n",
                (unsigned int)t->id, t->name,
                state_s, (void*)t->rsp, mark);
    }
}