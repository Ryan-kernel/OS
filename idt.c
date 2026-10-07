typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef unsigned long  uint64_t;

struct idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;
    uint8_t  type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

struct idt_entry idt_table[256];
struct idt_ptr   idt_descriptor;

extern void idt_load(void);
extern void keyboard_handler_stub(void);
extern void pit_handler_stub(void);
extern uint64_t isr_stub_table[];

extern void putchar(char c);
extern void print(const char *s);

extern void shell_input(char c);
extern void shell_enter(void);
extern void shell_backspace(void);

extern void pit_init(void);

static void idt_set_gate(int n, uint64_t handler) {
    idt_table[n].offset_low  = (uint16_t)(handler & 0xFFFF);
    idt_table[n].selector    = 0x18;         /* 64 位代码段选择子 */
    idt_table[n].ist         = 0;
    idt_table[n].type_attr   = 0x8E;         /* P=1, DPL=0, 64 位中断门 */
    idt_table[n].offset_mid  = (uint16_t)((handler >> 16) & 0xFFFF);
    idt_table[n].offset_high = (uint32_t)((handler >> 32) & 0xFFFFFFFF);
    idt_table[n].reserved    = 0;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static void pic_remap(void) {
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20);   /* 主 PIC 偏移 0x20 */
    outb(0xA1, 0x28);   /* 从 PIC 偏移 0x28 */
    outb(0x21, 0x04);   /* 主 PIC IRQ2 接从 PIC */
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    outb(0x21, 0xFC);   /* 打开 IRQ0、IRQ1 */
    outb(0xA1, 0xFF);
}

static int shift_left   = 0;
static int shift_right  = 0;
static int ctrl_left    = 0;
static int ctrl_right   = 0;
static int alt_left     = 0;
static int alt_right    = 0;
static int caps_lock    = 0;
static int num_lock     = 1;
static int extended     = 0;

static int shift_down(void) { return shift_left || shift_right; }

static const char __attribute__((section(".rodata"))) scancode_lower[] = {
    0,  0, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,  'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,  '\\','z','x','c','v','b','n','m',',','.','/',
    0,  '*', 0,  ' ',
    0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  '7','8','9',
    '-','4','5','6','+','1','2','3','0','.','/', 0,  0,
    0
};

static const char __attribute__((section(".rodata"))) scancode_upper[] = {
    0,  0, '!','@','#','$','%','^','&','*','(',')','_','+', '\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,  'A','S','D','F','G','H','J','K','L',':','"','~',
    0,  '|','Z','X','C','V','B','N','M','<','>','?',
    0,  '*', 0,  ' ',
    0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  '7','8','9',
    '-','4','5','6','+','1','2','3','0','.','/', 0,  0,
    0
};

static void handle_normal(uint8_t sc) {
    if (sc == 0x2A) { shift_left  = 1; return; }
    if (sc == 0x36) { shift_right = 1; return; }
    if (sc == 0x1D) { ctrl_left   = 1; return; }
    if (sc == 0x38) { alt_left    = 1; return; }
    if (sc == 0x3A) { caps_lock = !caps_lock; return; }
    if (sc == 0x45) { num_lock  = !num_lock;  return; }

    if (sc < sizeof(scancode_lower)) {
        char c;
        int upper = shift_down() ^ caps_lock;
        char base = scancode_lower[sc];

        if (base >= 'a' && base <= 'z') {
            c = upper ? scancode_upper[sc] : scancode_lower[sc];
        } else {
            c = shift_down() ? scancode_upper[sc] : scancode_lower[sc];
        }

        if (c == '\n')      shell_enter();
        else if (c == '\b') shell_backspace();
        else if (c)         shell_input(c);
    }
}

static void handle_extended(uint8_t sc) {
    if (sc == 0x1D) { ctrl_right  = 1; return; }
    if (sc == 0x38) { alt_right   = 1; return; }
    if (sc == 0x36) { shift_right = 1; return; }
    if (sc == 0x45) { num_lock = !num_lock; return; }

    if (sc == 0x1C) { shell_enter(); return; }
    if (sc == 0x35) { shell_input('/'); return; }

    if (num_lock) {
        static const uint8_t numpad_map[] = {
            0x47, 0x48, 0x49, 0x4B, 0x4C, 0x4D,
            0x4F, 0x50, 0x51, 0x52, 0x53, 0x37, 0x4E
        };
        static const char numpad_chars[] = {
            '7','8','9','4','5','6','1','2','3','0','.','*','+'
        };
        for (int i = 0; i < 13; i++) {
            if (sc == numpad_map[i]) {
                shell_input(numpad_chars[i]);
                return;
            }
        }
    }
}

void keyboard_handler(void) {
    uint8_t sc = inb(0x60);

    if (sc == 0xE0) { extended = 1; outb(0x20, 0x20); return; }

    if (extended) {
        extended = 0;
        if (sc & 0x80) {
            uint8_t rel = sc & 0x7F;
            if (rel == 0x1D) ctrl_right  = 0;
            if (rel == 0x38) alt_right   = 0;
            if (rel == 0x36) shift_right = 0;
        } else {
            handle_extended(sc);
        }
        outb(0x20, 0x20);
        return;
    }

    if (sc & 0x80) {
        uint8_t rel = sc & 0x7F;
        if (rel == 0x2A) shift_left  = 0;
        if (rel == 0x36) shift_right = 0;
        if (rel == 0x1D) ctrl_left   = 0;
        if (rel == 0x38) alt_left    = 0;
        outb(0x20, 0x20);
        return;
    }

    handle_normal(sc);
    outb(0x20, 0x20);
}

void idt_init(void) {
    __asm__ volatile ("cli");

    for (int i = 0; i < 256; i++) {
        idt_table[i].offset_low  = 0;
        idt_table[i].selector    = 0;
        idt_table[i].ist         = 0;
        idt_table[i].type_attr   = 0;
        idt_table[i].offset_mid  = 0;
        idt_table[i].offset_high = 0;
        idt_table[i].reserved    = 0;
    }

    for (int i = 0; i < 32; i++) {
        idt_set_gate(i, isr_stub_table[i]);
    }

    idt_set_gate(0x20, (uint64_t)pit_handler_stub);
    idt_set_gate(0x21, (uint64_t)keyboard_handler_stub);

    idt_descriptor.limit = sizeof(idt_table) - 1;
    idt_descriptor.base  = (uint64_t)idt_table;
    idt_load();

    pic_remap();
    pit_init();

    __asm__ volatile ("sti");
}