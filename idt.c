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

static void idt_set_gate(int n, uint64_t handler) {
    idt_table[n].offset_low  = (uint16_t)(handler & 0xFFFF);
    idt_table[n].selector    = 0x18;
    idt_table[n].ist         = 0;
    idt_table[n].type_attr   = 0x8E;
    idt_table[n].offset_mid  = (uint16_t)((handler >> 16) & 0xFFFF);
    idt_table[n].offset_high = (uint32_t)((handler >> 32) & 0xFFFFFFFF);
    idt_table[n].reserved    = 0;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static void pic_remap(void) {
    outb(0x20, 0x11);
    outb(0xA0, 0x11);
    outb(0x21, 0x20);
    outb(0xA1, 0x28);
    outb(0x21, 0x04);
    outb(0xA1, 0x02);
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    
    outb(0x21, 0xFE);
    outb(0xA1, 0xFF); 
}

extern void pit_init(void);

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
}