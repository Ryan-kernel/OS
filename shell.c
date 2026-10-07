typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef unsigned long  uint64_t;

#define BUF_SIZE  256
#define HIST_SIZE 16

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
extern void fs_ls(void);
extern void fs_touch(const char *name);
extern void fs_write(const char *name, const char *text);
extern void fs_cat(const char *name);
extern void fs_rm(const char *name);
extern void fs_mv(const char *old_name, const char *new_name);

static char input_buf[BUF_SIZE];
static int  input_len = 0;

static char history[HIST_SIZE][BUF_SIZE];
static int  hist_count = 0;

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

void shell_prompt(void) {
    print("> ");
}

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

static void execute(const char *cmd) {
    if (cmd[0] == 0) return;

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
        print("  shutdown     - power off\n");
        print("  poweroff     - alias for shutdown\n");
        print("  exit         - alias for shutdown\n");
        print("  mem          - show memory info\n");
        print("  cpu          - show CPU info\n");
        print("  heap         - show heap info\n");
        print("  alloc N      - allocate N bytes\n");
        print("  free 0xADDR  - free a block\n");
        print("  ls           - list files\n");
        print("  touch NAME   - create file\n");
        print("  write N TEXT - write text to file\n");
        print("  cat NAME     - show file contents\n");
        print("  rm NAME      - remove file\n");
        print("  mv OLD NEW   - rename file\n");
    } else if (str_eq(cmd, "clear")) {
        clear_screen();
    } else if (str_eq(cmd, "echo")) {
        putchar('\n');
    } else if (str_starts(cmd, "echo ")) {
        kprintf("%s\n", cmd + 5);
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
        kprintf("Time: ");
        if (h < 10) putchar('0');
        kprintf("%d:", h);
        if (m < 10) putchar('0');
        kprintf("%d:", m);
        if (s < 10) putchar('0');
        kprintf("%d\n", s);
    } else if (str_starts(cmd, "sleep ")) {
        const char *p = cmd + 6;
        uint32_t n = 0;
        while (*p >= '0' && *p <= '9') {
            n = n * 10 + (uint32_t)(*p - '0');
            p++;
        }
        if (n == 0) {
            kprintf("Usage: sleep N\n");
        } else {
            kprintf("Sleeping %u seconds...\n", n);
            pit_sleep(n);
            kprintf("Done.\n");
        }
    } else if (str_eq(cmd, "about")) {
        kprintf("OS 64-bit\n");
        kprintf("made with Ryan\n");
    } else if (str_eq(cmd, "hist")) {
        for (int i = 0; i < hist_count; i++) {
            kprintf("%s\n", history[i]);
        }
    } else if (str_eq(cmd, "reboot")) {
        kprintf("Rebooting...\n");
        reboot_now();
    } else if (str_eq(cmd, "shutdown") || str_eq(cmd, "poweroff") || str_eq(cmd, "exit")) {
        kprintf("Shutting down...\n");
        shutdown_now();
    } else if (str_eq(cmd, "mem")) {
        kprintf("Kernel loaded at: %p\n", (void*)0x10000);
        kprintf("BSS start: %p\n", (void*)&__bss_start);
        kprintf("BSS end:   %p\n", (void*)&__bss_end);
        kprintf("Ticks: %u\n", (unsigned int)pit_seconds());
    } else if (str_eq(cmd, "cpu")) {
        print_cpu_info();
    } else if (str_eq(cmd, "heap")) {
        heap_info();
    } else if (str_starts(cmd, "alloc ")) {
        const char *p = cmd + 6;
        uint64_t n = 0;
        while (*p >= '0' && *p <= '9') {
            n = n * 10 + (uint64_t)(*p - '0');
            p++;
        }
        if (n == 0) {
            kprintf("Usage: alloc N\n");
        } else {
            void *ptr = kmalloc(n);
            if (ptr) {
                kprintf("Allocated %u bytes at %p\n",
                        (unsigned int)n, ptr);
            } else {
                kprintf("Out of heap memory\n");
            }
        }
    } else if (str_starts(cmd, "free ")) {
        const char *p = cmd + 5;
        uint64_t addr = 0;
        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
            p += 2;
            while (1) {
                char c = *p;
                if (c >= '0' && c <= '9') addr = addr * 16 + (uint64_t)(c - '0');
                else if (c >= 'a' && c <= 'f') addr = addr * 16 + (uint64_t)(c - 'a' + 10);
                else if (c >= 'A' && c <= 'F') addr = addr * 16 + (uint64_t)(c - 'A' + 10);
                else break;
                p++;
            }
        }
        if (addr == 0) {
            kprintf("Usage: free 0xADDR\n");
        } else {
            kfree((void *)addr);
            kprintf("Freed %p\n", (void*)addr);
        }
    } else if (str_eq(cmd, "ls")) {
        fs_ls();
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
            char name[32];
            int n = 0;
            while (name_start + n < p && n < 31) {
                name[n] = name_start[n];
                n++;
            }
            name[n] = 0;
            p++;
            fs_write(name, p);
        } else {
            kprintf("Usage: write name text\n");
        }
    } else if (str_starts(cmd, "mv ")) {
        char old_name[32];
        char new_name[32];
        const char *p = cmd + 3;
        int n = 0;

        while (*p && *p != ' ' && n < 31) {
            old_name[n] = *p;
            n++;
            p++;
        }
        old_name[n] = 0;

        while (*p == ' ') p++;

        n = 0;
        while (*p && n < 31) {
            new_name[n] = *p;
            n++;
            p++;
        }
        new_name[n] = 0;

        if (old_name[0] == 0 || new_name[0] == 0) {
            kprintf("Usage: mv OLD NEW\n");
        } else {
            fs_mv(old_name, new_name);
        }
    } else {
        kprintf("Unknown command: %s\n", cmd);
    }
}

void shell_enter(void) {
    putchar('\n');
    input_buf[input_len] = 0;
    hist_push(input_buf);
    execute(input_buf);
    input_len = 0;
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

void shell_init(void) {
    input_len = 0;
    hist_count = 0;

    print("OS Shell\n");
    print("Type 'help' for commands.\n\n");
    shell_prompt();
}