typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef unsigned long  uint64_t;

#define BUF_SIZE  256
#define HIST_SIZE 16

extern void putchar(char c);
extern void print(const char *s);
extern void print_hex(uint64_t n);
extern void print_dec(int n);
extern void clear_screen(void);
extern uint64_t pit_seconds(void);

extern void reboot_now(void);
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
        print("  time         - show uptime\n");
        print("  about        - about this OS\n");
        print("  hist         - show history\n");
        print("  reboot       - reboot the machine\n");
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
    } else if (str_eq(cmd, "clear")) {
        clear_screen();
    } else if (str_starts(cmd, "echo ")) {
        print(cmd + 5);
        putchar('\n');
    } else if (str_eq(cmd, "time")) {
        print("Uptime: ");
        print_dec((int)pit_seconds());
        print(" seconds\n");
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
    } else if (str_eq(cmd, "mem")) {
        print("Kernel loaded at: ");
        print_hex(0x8000);
        putchar('\n');
        print("BSS start: ");
        print_hex((uint64_t)&__bss_start);
        putchar('\n');
        print("BSS end:   ");
        print_hex((uint64_t)&__bss_end);
        putchar('\n');
        print("Ticks: ");
        print_dec((int)pit_seconds());
        putchar('\n');
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
            print("Usage: alloc N\n");
        } else {
            void *ptr = kmalloc(n);
            if (ptr) {
                print("Allocated ");
                print_dec((int)n);
                print(" bytes at ");
                print_hex((uint64_t)ptr);
                putchar('\n');
            } else {
                print("Out of heap memory\n");
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
            print("Usage: free 0xADDR\n");
        } else {
            kfree((void *)addr);
            print("Freed 0x");
            print_hex(addr);
            putchar('\n');
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
            print("Usage: write name text\n");
        }
    } else {
        print("Unknown command: ");
        print(cmd);
        putchar('\n');
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