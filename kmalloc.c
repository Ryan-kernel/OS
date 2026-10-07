typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef unsigned long  uint64_t;

extern void kprintf(const char *fmt, ...);

#define HEAP_START  0x200000ULL
#define HEAP_END    0x400000ULL
#define ALIGN8(x)   (((x) + 7) & ~7ULL)
#define BLOCK_HEADER  sizeof(struct free_block)

struct free_block {
    uint64_t size;
    struct free_block *next;
};

static struct free_block *free_list = 0;

void heap_init(void) {
    free_list = (struct free_block *)HEAP_START;
    free_list->size = HEAP_END - HEAP_START;
    free_list->next = 0;
}

void *kmalloc(uint64_t size) {
    if (size == 0) return 0;
    size = ALIGN8(size);

    struct free_block *prev = 0;
    struct free_block *cur = free_list;

    while (cur) {
        if (cur->size >= size + BLOCK_HEADER) {
            uint64_t remaining = cur->size - size - BLOCK_HEADER;

            if (remaining >= BLOCK_HEADER + 8) {
                struct free_block *new_block =
                    (struct free_block *)((uint64_t)cur + BLOCK_HEADER + size);
                new_block->size = remaining;
                new_block->next = cur->next;

                if (prev) prev->next = new_block;
                else free_list = new_block;

                cur->size = size + BLOCK_HEADER;
                return (void *)((uint64_t)cur + BLOCK_HEADER);
            } else {
                if (prev) prev->next = cur->next;
                else free_list = cur->next;
                cur->size = size + BLOCK_HEADER;
                return (void *)((uint64_t)cur + BLOCK_HEADER);
            }
        }
        prev = cur;
        cur = cur->next;
    }

    return 0;
}

void kfree(void *ptr) {
    if (!ptr) return;

    struct free_block *block =
        (struct free_block *)((uint64_t)ptr - BLOCK_HEADER);

    struct free_block *prev = 0;
    struct free_block *cur = free_list;

    while (cur && cur < block) {
        prev = cur;
        cur = cur->next;
    }

    block->next = cur;
    if (prev) prev->next = block;
    else free_list = block;

    if (cur && (uint64_t)block + block->size == (uint64_t)cur) {
        block->size += cur->size;
        block->next = cur->next;
    }
    if (prev && (uint64_t)prev + prev->size == (uint64_t)block) {
        prev->size += block->size;
        prev->next = block->next;
    }
}

uint64_t heap_free(void) {
    uint64_t total = 0;
    struct free_block *cur = free_list;
    while (cur) {
        total += cur->size;
        cur = cur->next;
    }
    return total;
}

uint64_t heap_used(void) {
    return (HEAP_END - HEAP_START) - heap_free();
}

void heap_info(void) {
    kprintf("Heap start: %p\n", (void*)HEAP_START);
    kprintf("Heap end:   %p\n", (void*)HEAP_END);
    kprintf("Heap used:  %u bytes\n", (unsigned int)heap_used());
    kprintf("Heap free:  %u bytes\n", (unsigned int)heap_free());
}