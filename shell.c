typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef unsigned long  uint64_t;

#define BUF_SIZE  256
#define HIST_SIZE 16

/* ============================================================
 *  外部依赖
 * ============================================================ */
extern void putchar(char c);
extern void print(const char *s);
extern void kprintf(const char *fmt, ...);
extern void clear_screen(void);
extern uint64_t pit_seconds(void);
extern void pit_get_time(int *hours, int *minutes, int *seconds);
extern void pit_sleep(uint32_t seconds);

extern uint32_t div_u32(uint32_t a, uint32_t b);
extern uint32_t mod_u32(uint32_t a, uint32_t b);

extern void reboot_now(void);
extern void shutdown_now(void);
extern void print_cpu_info(void);
extern char __bss_start;
extern char __bss_end;
extern void heap_info(void);
extern void *kmalloc(uint64_t size);
extern void kfree(void *ptr);

/* FS 接口 */
extern void fs_init(void);
extern void fs_ls(void);
extern void fs_touch(const char *name);
extern void fs_write(const char *name, const char *text);
extern void fs_cat(const char *name);
extern void fs_rm(const char *name);
extern void fs_mv(const char *old_name, const char *new_name);
extern void fs_mkdir(const char *path);
extern void fs_cd(const char *path);
extern void fs_pwd(void);

/* 任务接口 */
extern void task_list(void);
extern int  task_create(const char *name, void (*entry)(void));
extern void task_exit(void);
extern void task_sleep(uint32_t seconds);

/* ============================================================
 *  输入缓冲 / 历史
 * ============================================================ */
static char input_buf[BUF_SIZE];
static int  input_len = 0;

static char history[HIST_SIZE][BUF_SIZE];
static int  hist_count = 0;

/* ============================================================
 *  后台计数器任务（不打印，只改全局变量）
 * ============================================================ */
static volatile int g_counter_value = 0;

void counter_task(void) {
    /* 跑 10 次后自动退出，演示 task_exit */
    for (int i = 0; i < 10; i++) {
        g_counter_value++;
        task_sleep(1);
    }
    /* 退出前打印一次，告诉用户 */
    kprintf("\n[counter] task finished and exited.\n");
    task_exit();
}

/* ============================================================
 *  字符串工具
 * ============================================================ */
static int str_eq(const char *a, const char *b) {
    while (*a && *b) { if (*a != *b) return 0; a++; b++; }
    return *a == *b;
}

static int str_starts(const char *s, const char *prefix) {
    while (*prefix) { if (*s != *prefix) return 0; s++; prefix++; }
    return 1;
}

static void str_copy(char *dst, const char *src, int max) {
    int i = 0;
    while (src[i] && i < max - 1) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

/* ============================================================
 *  提示符
 * ============================================================ */
void shell_prompt(void) {
    print("> ");
}

/* ============================================================
 *  历史
 * ============================================================ */
static void hist_push(const char *cmd) {
    if (cmd[0] == 0) return;
    if (hist_count > 0 && str_eq(history[hist_count - 1], cmd)) return;
    if (hist_count < HIST_SIZE) {
        str_copy(history[hist_count], cmd, BUF_SIZE);
        hist_count++;
    } else {
        for (int i = 1; i < HIST_SIZE; i++)
            str_copy(history[i - 1], history[i], BUF_SIZE);
        str_copy(history[HIST_SIZE - 1], cmd, BUF_SIZE);
    }
}

/* ============================================================
 *  命令执行
 * ============================================================ */
static void execute(const char *cmd) {
    if (cmd[0] == 0) return;

    // 引入 kprintf 防止没有 print_dec
    extern void kprintf(const char *fmt, ...);

    if (str_eq(cmd, "help")) {
        print("Commands:\n");
        print("  help         - show this\n");
        print("  clear        - clear screen\n");
        print("  echo X       - print X\n");
        print("  time         - show uptime in seconds\n");
        print("  uptime       - show uptime\n");
        print("  date         - show current time\n");
        print("  sleep N      - sleep N seconds\n");
        print("  about        - about this OS\n");
        print("  hist         - show history\n");
        print("  reboot       - reboot the machine\n");
        print("  shutdown     - power off (QEMU isa-debug-exit)\n");
        print("  exit         - alias for shutdown\n");
        print("  ls           - list files\n");
        print("  mkdir NAME   - create directory\n");
        print("  cd PATH      - change directory\n");
        print("  pwd          - print working directory\n");
        print("  touch NAME   - create file\n");
        print("  write N TEXT - write text to file\n");
        print("  cat NAME     - show file contents\n");
        print("  rm NAME      - remove file\n");
    } else if (str_eq(cmd, "clear")) {
        clear_screen();
    } else if (str_eq(cmd, "echo")) {
        putchar('\n');
    } else if (str_starts(cmd, "echo ")) {
        print(cmd + 5);
        putchar('\n');
    } else if (str_eq(cmd, "time")) {
        kprintf("Uptime: %u seconds\n", (unsigned int)pit_seconds());
    } else if (str_eq(cmd, "uptime")) {
        uint32_t sec = (uint32_t)pit_seconds();
        uint32_t h = div_u32(sec, 3600);
        uint32_t m = mod_u32(div_u32(sec, 60), 60);
        uint32_t s = mod_u32(sec, 60);
        kprintf("Uptime: %uh %um %us\n", h, m, s);
    } else if (str_eq(cmd, "date")) {
        int h, m, s;
        pit_get_time(&h, &m, &s);
        kprintf("Time: %02d:%02d:%02d\n", h, m, s); // 这里之前可能会乱码，我改成了简单版
        // 如果上面的 %02d 还是报错，请换成下面这三行：
        // print("Time: "); kprintf("%d:", h); kprintf("%d:", m); kprintf("%d\n", s);
    } else if (str_eq(cmd, "about")) {
        print("OS 64-bit\n");
        print("made with Ryan\n");
    } else if (str_eq(cmd, "hist")) {
        for (int i = 0; i < hist_count; i++) {
            print(history[i]);
            putchar('\n');
        }
    } else if (str_eq(cmd, "reboot")) {
        print("Rebooting...\n");
        reboot_now();
    } else if (str_eq(cmd, "shutdown") || str_eq(cmd, "poweroff") || str_eq(cmd, "exit")) {
        print("Shutting down...\n");
        shutdown_now();
    } else if (str_eq(cmd, "ls")) {
        fs_ls();
    } else if (str_starts(cmd, "mkdir ")) {
        fs_mkdir(cmd + 6);
    } else if (str_starts(cmd, "cd ")) {
        fs_cd(cmd + 3);
    } else if (str_eq(cmd, "cd")) {
        fs_cd("/");
    } else if (str_eq(cmd, "pwd")) {
        fs_pwd();
    } else if (str_starts(cmd, "touch ")) {
        fs_touch(cmd + 6);
    } else if (str_starts(cmd, "cat ")) {
        fs_cat(cmd + 4);
    } else if (str_starts(cmd, "rm ")) {
        fs_rm(cmd + 3);
    } else if (str_starts(cmd, "write ")) {
        const char *p = cmd + 6;
        const char *name_start = p;
        while (*p && *p != ' ') p++;
        if (*p == ' ') {
            char name[64];
            int n = 0;
            while (name_start + n < p && n < 63) { name[n] = name_start[n]; n++; }
            name[n] = 0;
            p++;
            fs_write(name, p);
        } else {
            print("Usage: write name text\n");
        }
    } else {
        print("Unknown command: ");
        int len = 0;
        while (cmd[len]) {
            putchar(cmd[len]);
            len++;
        }
        putchar('\n');
    }
}

/* ============================================================
 *  输入处理
 * ============================================================ */
void shell_enter(void) {
    putchar('\n');
    
    // 确保字符串正常结尾
    input_buf[input_len] = 0;
    
    // 只有长度大于 0，才执行命令
    if (input_len > 0) {
        execute(input_buf);
        hist_push(input_buf);
    }
    
    // 极其重要：彻底清空缓冲区，不留任何垃圾字符
    input_len = 0;
    for (int i = 0; i < BUF_SIZE; i++) {
        input_buf[i] = 0;
    }
    
    shell_prompt();
}

void shell_backspace(void) {
    if (input_len > 0) {
        input_len--;
        putchar('\b');
    }
}

void shell_input(char c) {
    if (input_len < BUF_SIZE - 1) {
        input_buf[input_len++] = c;
        putchar(c);
    }
}

/* ============================================================
 *  初始化
 * ============================================================ */
void shell_init(void) {
    input_len = 0;
    hist_count = 0;

    clear_screen();
    print("OS Shell\n");
    print("Type 'help' for commands.\n\n");
}