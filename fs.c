typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef unsigned long  uint64_t;

extern void kprintf(const char *fmt, ...);
extern void *kmalloc(uint64_t size);
extern void kfree(void *ptr);

#define MAX_FILES     16
#define NAME_LEN      32
#define MAX_FILE_SIZE 256

struct file {
    char  name[NAME_LEN];
    char *data;
    int   size;
    int   used;
};

static struct file files[MAX_FILES];

static int str_eq(const char *a, const char *b) {
    while (*a && *b) { if (*a != *b) return 0; a++; b++; }
    return *a == *b;
}

static void str_copy(char *dst, const char *src, int max) {
    int i = 0;
    while (src[i] && i < max - 1) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

void fs_init(void) {
    for (int i = 0; i < MAX_FILES; i++) {
        files[i].used = 0;
        files[i].name[0] = 0;
        files[i].data = 0;
        files[i].size = 0;
    }
}

static struct file *fs_find(const char *name) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used && str_eq(files[i].name, name)) {
            return &files[i];
        }
    }
    return 0;
}

void fs_touch(const char *name) {
    if (name[0] == 0) {
        kprintf("Usage: touch NAME\n");
        return;
    }
    if (fs_find(name)) {
        kprintf("File already exists: %s\n", name);
        return;
    }
    for (int i = 0; i < MAX_FILES; i++) {
        if (!files[i].used) {
            files[i].used = 1;
            str_copy(files[i].name, name, NAME_LEN);
            files[i].data = 0;
            files[i].size = 0;
            kprintf("Created: %s\n", name);
            return;
        }
    }
    kprintf("No free file slots\n");
}

void fs_ls(void) {
    int count = 0;
    kprintf("Files:\n");
    for (int i = 0; i < MAX_FILES; i++) {
        if (files[i].used) {
            kprintf("  %s  (%d bytes)\n", files[i].name, files[i].size);
            count++;
        }
    }
    if (count == 0) {
        kprintf("  (none)\n");
    }
}

void fs_write(const char *name, const char *text) {
    struct file *f = fs_find(name);
    if (!f) {
        kprintf("File not found: %s\n", name);
        return;
    }

    int len = 0;
    while (text[len]) len++;
    if (len > MAX_FILE_SIZE) len = MAX_FILE_SIZE;

    if (f->data) {
        kfree(f->data);
        f->data = 0;
        f->size = 0;
    }

    if (len == 0) {
        kprintf("Wrote 0 bytes to %s\n", name);
        return;
    }

    f->data = (char *)kmalloc(len + 1);
    if (!f->data) {
        kprintf("Out of memory\n");
        return;
    }
    str_copy(f->data, text, len + 1);
    f->size = len;

    kprintf("Wrote %d bytes to %s\n", len, name);
}

void fs_cat(const char *name) {
    struct file *f = fs_find(name);
    if (!f) {
        kprintf("File not found: %s\n", name);
        return;
    }
    if (f->size == 0 || !f->data) {
        kprintf("(empty)\n");
        return;
    }
    kprintf("%s\n", f->data);
}

void fs_rm(const char *name) {
    struct file *f = fs_find(name);
    if (!f) {
        kprintf("File not found: %s\n", name);
        return;
    }
    if (f->data) kfree(f->data);
    f->used = 0;
    f->data = 0;
    f->size = 0;
    f->name[0] = 0;
    kprintf("Removed: %s\n", name);
}

void fs_mv(const char *old_name, const char *new_name) {
    struct file *f = fs_find(old_name);
    if (!f) {
        kprintf("File not found: %s\n", old_name);
        return;
    }
    if (fs_find(new_name)) {
        kprintf("Target already exists: %s\n", new_name);
        return;
    }
    str_copy(f->name, new_name, NAME_LEN);
    kprintf("Renamed: %s -> %s\n", old_name, new_name);
}