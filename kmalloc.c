typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef unsigned long  uint64_t;

extern void print(const char *s);
extern void print_hex(uint64_t n);
extern void print_dec(int n);
extern void putchar(char c);

#define HEAP_START  0x100000
#define HEAP_END    0x200000
#define ALIGN8(x)   (((x) + 7) & ~7ULL)
#define BLOCK_HEADER  sizeof(struct free_block)

/* 空闲块结构 */
struct free_block {
    uint64_t size;              /* 块大小（含头部） */
    struct free_block *next;    /* 下一个空闲块 */
};

static struct free_block *free_list = 0;

/* 初始化堆 */
void heap_init(void) {
    free_list = (struct free_block *)HEAP_START;
    free_list->size = HEAP_END - HEAP_START;
    free_list->next = 0;
}

/* 分配 */
void *kmalloc(uint64_t size) {
    if (size == 0) return 0;
    size = ALIGN8(size);

    struct free_block *prev = 0;
    struct free_block *cur = free_list;

    while (cur) {
        if (cur->size >= size + BLOCK_HEADER) {
            uint64_t remaining = cur->size - size - BLOCK_HEADER;

            if (remaining >= BLOCK_HEADER + 8) {
                /* 切分：cur 给用户，new_block 留在 free_list */
                struct free_block *new_block =
                    (struct free_block *)((uint64_t)cur + BLOCK_HEADER + size);
                new_block->size = remaining;
                new_block->next = cur->next;

                /* cur 从 free_list 移除，new_block 替代它 */
                if (prev) prev->next = new_block;
                else free_list = new_block;

                cur->size = size + BLOCK_HEADER;
                return (void *)((uint64_t)cur + BLOCK_HEADER);
            } else {
                /* 整块给出 */
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

/* 释放 */
void kfree(void *ptr) {
    if (!ptr) return;

    struct free_block *block =
        (struct free_block *)((uint64_t)ptr - BLOCK_HEADER);

    /* 按地址顺序插入 free_list */
    struct free_block *prev = 0;
    struct free_block *cur = free_list;

    while (cur && cur < block) {
        prev = cur;
        cur = cur->next;
    }

    block->next = cur;
    if (prev) prev->next = block;
    else free_list = block;

    /* 合并相邻空闲块 */
    if (cur && (uint64_t)block + block->size == (uint64_t)cur) {
        block->size += cur->size;
        block->next = cur->next;
    }
    if (prev && (uint64_t)prev + prev->size == (uint64_t)block) {
        prev->size += block->size;
        prev->next = block->next;
    }
}

/* 统计空闲字节数 */
uint64_t heap_free(void) {
    uint64_t total = 0;
    struct free_block *cur = free_list;
    while (cur) {
        total += cur->size;
        cur = cur->next;
    }
    return total;
}

/* 统计已用字节数 */
uint64_t heap_used(void) {
    return (HEAP_END - HEAP_START) - heap_free();
}

/* 打印堆信息 */
void heap_info(void) {
    print("Heap start: 0x");
    print_hex(HEAP_START);
    putchar('\n');
    print("Heap end:   0x");
    print_hex(HEAP_END);
    putchar('\n');
    print("Heap used:  ");
    print_dec((int)heap_used());
    print(" bytes\n");
    print("Heap free:  ");
    print_dec((int)heap_free());
    print(" bytes\n");
}