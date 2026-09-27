typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef unsigned long  uint64_t;

#define MAX_FILES    16
#define NAME_LEN     32
#define MAX_FILE_SIZE 256

extern void *kmalloc(uint64_t size);
extern void kfree(void *ptr);
extern void print(const char *s);
extern void print_dec(int n);
extern void putchar(char c);

struct file {
    char  name[NAME_LEN];
    char *data;
    int   size;
    int   used;
};

static struct file files[MAX_FILES];

/* 字符串工具 */
static int str_eq(const char *a, const char *b) {
    while (*a && *b) { if (*a != *b) return 0; a++; b++; }
    return *a == *b;
}

static void str_copy(char *dst, const char *src, int max) {
    int i = 0;
    while (src[i] && i < max - 1) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

/* 初始化文件系统 */
void fs_init(void) {
    for (int i = 0; i < MAX_FILES; i++) {
        files[i].used = 0;
        files[i].name[0] = 0;
        files[i].data = 0;
        files[i].size = 0;
    }
}

/* 查找文件 */
static struct file *fs_find(const char *name) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used && str_eq(files[i].name, name)) {
            return &files[i];
        }
    }
    return 0;
}

/* 创建文件 */
void fs_touch(const char *name) {
    if (fs_find(name)) {
        print("File already exists: ");
        print(name);
        putchar('\n');
        return;
    }
    for (int i = 0; i < MAX_FILES; i++) {
        if (!files[i].used) {
            files[i].used = 1;
            str_copy(files[i].name, name, NAME_LEN);
            files[i].data = 0;
            files[i].size = 0;
            print("Created: ");
            print(name);
            putchar('\n');
            return;
        }
    }
    print("No free file slots\n");
}

/* 列出文件 */
void fs_ls(void) {
    int count = 0;
    print("Files:\n");
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used) {
            print("  ");
            print(files[i].name);
            print("  (");
            print_dec(files[i].size);
            print(" bytes)\n");
            count++;
        }
    }
    if (count == 0) {
        print("  (none)\n");
    }
}

/* 写入文件 */
void fs_write(const char *name, const char *text) {
    struct file *f = fs_find(name);
    if (!f) {
        print("File not found: ");
        print(name);
        putchar('\n');
        return;
    }

    int len = 0;
    while (text[len]) len++;
    if (len > MAX_FILE_SIZE) len = MAX_FILE_SIZE;

    if (f->data) kfree(f->data);
    f->data = (char *)kmalloc(len + 1);
    if (!f->data) {
        print("Out of memory\n");
        return;
    }
    str_copy(f->data, text, len + 1);
    f->size = len;

    print("Wrote ");
    print_dec(len);
    print(" bytes to ");
    print(name);
    putchar('\n');
}

/* 读取文件 */
void fs_cat(const char *name) {
    struct file *f = fs_find(name);
    if (!f) {
        print("File not found: ");
        print(name);
        putchar('\n');
        return;
    }
    if (f->size == 0 || !f->data) {
        print("(empty)\n");
        return;
    }
    print(f->data);
    putchar('\n');
}

/* 删除文件 */
void fs_rm(const char *name) {
    struct file *f = fs_find(name);
    if (!f) {
        print("File not found: ");
        print(name);
        putchar('\n');
        return;
    }
    if (f->data) kfree(f->data);
    f->used = 0;
    f->data = 0;
    f->size = 0;
    f->name[0] = 0;
    print("Removed: ");
    print(name);
    putchar('\n');
}