typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef unsigned long  uint64_t;

extern void kprintf(const char *fmt, ...);
extern void putchar(char c);
extern void *kmalloc(uint64_t size);
extern void kfree(void *ptr);

/* ============================================================
 *  ATA PIO 接口（保持不变）
 * ============================================================ */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}
static inline void outw(uint16_t port, uint16_t val) {
    __asm__ volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

#define ATA_DATA        0x1F0
#define ATA_SECCOUNT    0x1F2
#define ATA_LBA_LOW     0x1F3
#define ATA_LBA_MID     0x1F4
#define ATA_LBA_HIGH    0x1F5
#define ATA_DRIVE_HEAD  0x1F6
#define ATA_STATUS      0x1F7
#define ATA_COMMAND     0x1F7
#define ATA_ALT_STATUS  0x3F6

#define ATA_SR_BSY      0x80
#define ATA_SR_DRQ      0x08
#define ATA_SR_ERR      0x01

#define ATA_CMD_READ    0x20
#define ATA_CMD_WRITE   0x30
#define ATA_CMD_IDENTIFY 0xEC
#define ATA_CMD_FLUSH   0xE7

static void ata_delay(void) {
    for (int i = 0; i < 4; i++) inb(ATA_ALT_STATUS);
}

static int ata_wait_bsy(void) {
    for (uint32_t i = 0; i < 100000000; i++) {
        uint8_t st = inb(ATA_STATUS);
        if (!(st & ATA_SR_BSY)) return st;
    }
    return -1;
}

static int ata_wait_drq(void) {
    for (uint32_t i = 0; i < 100000000; i++) {
        uint8_t st = inb(ATA_STATUS);
        if (st & ATA_SR_BSY) continue;
        if (st & ATA_SR_ERR) return -1;
        if (st & ATA_SR_DRQ) return st;
    }
    return -1;
}

static void ata_setup_lba(uint32_t lba, uint8_t count) {
    outb(ATA_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_SECCOUNT,   count);
    outb(ATA_LBA_LOW,    (uint8_t)(lba & 0xFF));
    outb(ATA_LBA_MID,    (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_LBA_HIGH,   (uint8_t)((lba >> 16) & 0xFF));
}

static int ata_init(void) {
    outb(ATA_DRIVE_HEAD, 0xA0);
    ata_delay();
    outb(ATA_ALT_STATUS, 0);
    outb(ATA_SECCOUNT, 0);
    outb(ATA_LBA_LOW,  0);
    outb(ATA_LBA_MID,  0);
    outb(ATA_LBA_HIGH, 0);
    outb(ATA_COMMAND,  ATA_CMD_IDENTIFY);
    ata_delay();

    uint8_t st = inb(ATA_STATUS);
    if (st == 0) return -1;
    if (ata_wait_bsy() < 0) return -1;
    if (inb(ATA_LBA_MID) != 0 || inb(ATA_LBA_HIGH) != 0) return -1;

    for (int i = 0; i < 1000000; i++) {
        st = inb(ATA_STATUS);
        if (st & ATA_SR_ERR) return -1;
        if (st & ATA_SR_DRQ) break;
    }
    if (!(inb(ATA_STATUS) & ATA_SR_DRQ)) return -1;
    for (int i = 0; i < 256; i++) inw(ATA_DATA);
    return 0;
}

static int ata_read_sector(uint32_t lba, void *buf) {
    if (ata_wait_bsy() < 0) return -1;
    ata_setup_lba(lba, 1);
    outb(ATA_COMMAND, ATA_CMD_READ);
    if (ata_wait_drq() < 0) return -1;
    uint16_t *p = (uint16_t *)buf;
    for (int i = 0; i < 256; i++) *p++ = inw(ATA_DATA);
    ata_delay();
    return 0;
}

static int ata_write_sector(uint32_t lba, const void *buf) {
    if (ata_wait_bsy() < 0) return -1;
    ata_setup_lba(lba, 1);
    outb(ATA_COMMAND, ATA_CMD_WRITE);
    if (ata_wait_drq() < 0) return -1;
    const uint16_t *p = (const uint16_t *)buf;
    for (int i = 0; i < 256; i++) outw(ATA_DATA, *p++);
    ata_delay();
    outb(ATA_COMMAND, ATA_CMD_FLUSH);
    ata_wait_bsy();
    return 0;
}

/* ============================================================
 *  磁盘布局与位图
 * ============================================================ */
#define FS_MAGIC        0x4F534653
#define TOTAL_SECTORS   2048

#define SUPER_LBA       0
#define INODE_START_LBA 1
#define MAX_INODES      16
#define ROOT_INODE      0

#define DATA_START_LBA  24
#define INODE_TYPE_FILE 1
#define INODE_TYPE_DIR  2

#define MAX_EXTENTS     8

struct extent {
    uint32_t lba;
    uint32_t length;
} __attribute__((packed));

struct inode {
    uint32_t used;
    uint32_t type;
    uint32_t size;          
    uint32_t parent_inode;
    char     name[24];
    uint32_t extent_count;
    struct extent extents[MAX_EXTENTS];
} __attribute__((packed));

struct dir_entry {
    char     name[24];
    uint32_t inode_idx;
    uint32_t type;
} __attribute__((packed));

#define ENTRIES_PER_SECTOR (512 / sizeof(struct dir_entry))

/* 超级块：包含空闲位图 (256 字节 = 2048 位) */
struct superblock {
    uint32_t magic;
    uint8_t  bitmap[256];    /* 1 表示占用，0 表示空闲 */
    uint8_t  reserved[252];
} __attribute__((packed));

static struct superblock g_sb;
static uint32_t g_root_inode = ROOT_INODE;
static uint32_t g_cwd_inode  = ROOT_INODE;
static char     g_cwd_path[128] = "/";

static uint8_t  s_sector[512];
static char     s_parent[128];
static char     s_name[24];
static char     s_component[24];

/* ============================================================
 *  字符串工具
 * ============================================================ */
static int str_eq(const char *a, const char *b) {
    while (*a && *b) { if (*a != *b) return 0; a++; b++; }
    return *a == *b;
}
static void str_copy(char *dst, const char *src, int max) {
    int i = 0;
    while (src[i] && i < max - 1) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}
static int str_len(const char *s) { int n = 0; while (s[n]) n++; return n; }

/* ============================================================
 *  位图与磁盘同步
 * ============================================================ */
static void write_superblock(void) {
    for (int i = 0; i < 512; i++) s_sector[i] = 0;
    for (uint32_t i = 0; i < sizeof(struct superblock); i++) {
        s_sector[i] = ((uint8_t *)&g_sb)[i];
    }
    ata_write_sector(SUPER_LBA, s_sector);
}

static int is_bitmap_used(uint32_t lba) {
    if (lba >= TOTAL_SECTORS) return 1;
    return (g_sb.bitmap[lba / 8] & (1 << (lba % 8))) != 0;
}

static void set_bitmap(uint32_t lba, int used) {
    if (lba >= TOTAL_SECTORS) return;
    if (used) g_sb.bitmap[lba / 8] |= (1 << (lba % 8));
    else      g_sb.bitmap[lba / 8] &= ~(1 << (lba % 8));
}

/* 找连续 count 个空闲扇区，返回起始 LBA；-1 为失败 */
static int alloc_sectors(uint32_t count) {
    uint32_t start = 0;
    uint32_t run = 0;

    for (uint32_t i = DATA_START_LBA; i < TOTAL_SECTORS; i++) {
        if (!is_bitmap_used(i)) {
            if (run == 0) start = i;
            run++;
            if (run == count) {
                for (uint32_t j = 0; j < count; j++) set_bitmap(start + j, 1);
                write_superblock();
                return (int)start;
            }
        } else {
            run = 0;
        }
    }
    return -1;
}

static void free_sectors(uint32_t lba, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        set_bitmap(lba + i, 0);
    }
    write_superblock();
}

/* ============================================================
 *  Inode 与 目录 读写
 * ============================================================ */
static int read_inode(uint32_t idx, struct inode *out) {
    if (idx >= MAX_INODES) return -1;
    if (ata_read_sector(INODE_START_LBA + idx, s_sector) < 0) return -1;
    for (uint32_t i = 0; i < sizeof(struct inode); i++) {
        ((uint8_t *)out)[i] = s_sector[i];
    }
    return 0;
}

static int write_inode(uint32_t idx, const struct inode *in) {
    if (idx >= MAX_INODES) return -1;
    for (uint32_t i = 0; i < sizeof(struct inode); i++) {
        s_sector[i] = ((const uint8_t *)in)[i];
    }
    return ata_write_sector(INODE_START_LBA + idx, s_sector);
}

static int alloc_inode(uint32_t *out_idx) {
    for (uint32_t i = 1; i < MAX_INODES; i++) {
        struct inode in;
        if (read_inode(i, &in) < 0) return -1;
        if (in.used == 0) {
            *out_idx = i;
            return 0;
        }
    }
    return -1;
}

static int dir_add_entry(struct inode *dir, const char *name, uint32_t child_idx, uint32_t child_type) {
    uint32_t offset = dir->size * sizeof(struct dir_entry);
    uint32_t sector_idx = offset / 512;
    uint32_t sector_off = offset % 512;

    if (dir->extent_count == 0) {
        int lba = alloc_sectors(1);
        if (lba < 0) return -1;
        dir->extents[0].lba = lba;
        dir->extents[0].length = 1;
        dir->extent_count = 1;
        for (int i = 0; i < 512; i++) s_sector[i] = 0;
        ata_write_sector(lba, s_sector);
    }

    if (sector_idx >= dir->extents[0].length) return -1;

    uint32_t lba = dir->extents[0].lba + sector_idx;
    if (ata_read_sector(lba, s_sector) < 0) return -1;

    struct dir_entry *e = (struct dir_entry *)(s_sector + sector_off);
    str_copy(e->name, name, 24);
    e->inode_idx = child_idx;
    e->type = child_type;

    if (ata_write_sector(lba, s_sector) < 0) return -1;
    dir->size++;
    return 0;
}

static int dir_find_entry(struct inode *dir, const char *name, uint32_t *out_idx) {
    if (dir->extent_count == 0 || dir->size == 0) return -1;

    for (uint32_t i = 0; i < dir->size; i++) {
        uint32_t sector_idx = i / ENTRIES_PER_SECTOR;
        uint32_t sector_off = (i % ENTRIES_PER_SECTOR) * sizeof(struct dir_entry);
        if (sector_idx >= dir->extents[0].length) break;

        uint32_t lba = dir->extents[0].lba + sector_idx;
        if (ata_read_sector(lba, s_sector) < 0) return -1;

        struct dir_entry *e = (struct dir_entry *)(s_sector + sector_off);
        if (str_eq(e->name, name)) {
            *out_idx = e->inode_idx;
            return 0;
        }
    }
    return -1;
}

static int dir_remove_entry(struct inode *dir, const char *name) {
    if (dir->extent_count == 0 || dir->size == 0) return -1;

    for (uint32_t i = 0; i < dir->size; i++) {
        uint32_t sector_idx = i / ENTRIES_PER_SECTOR;
        uint32_t sector_off = (i % ENTRIES_PER_SECTOR) * sizeof(struct dir_entry);
        uint32_t lba = dir->extents[0].lba + sector_idx;

        ata_read_sector(lba, s_sector);
        struct dir_entry *e = (struct dir_entry *)(s_sector + sector_off);
        if (str_eq(e->name, name)) {
            e->inode_idx = 0; // 标记空
            e->name[0] = 0;
            ata_write_sector(lba, s_sector);
            return 0;
        }
    }
    return -1;
}

/* ============================================================
 *  路径解析与初始化
 * ============================================================ */
static uint32_t resolve_path(const char *path) {
    uint32_t cur = (path[0] == '/') ? g_root_inode : g_cwd_inode;
    const char *p = (path[0] == '/') ? (path + 1) : path;

    static struct inode cur_in;
    read_inode(cur, &cur_in);

    while (*p) {
        int n = 0;
        while (*p && *p != '/' && n < 23) s_component[n++] = *p++;
        s_component[n] = 0;
        while (*p == '/') p++;
        if (n == 0) continue;

        if (cur_in.type != INODE_TYPE_DIR) return (uint32_t)-1;

        uint32_t child;
        if (str_eq(s_component, "..")) {
            cur = cur_in.parent_inode;
        } else if (str_eq(s_component, ".")) {
            /* 不变 */
        } else {
            if (dir_find_entry(&cur_in, s_component, &child) < 0) return (uint32_t)-1;
            cur = child;
        }
        read_inode(cur, &cur_in);
    }
    return cur;
}

void fs_init(void) {
    if (ata_init() != 0) return;

    if (ata_read_sector(SUPER_LBA, s_sector) < 0) return;
    for (uint32_t i = 0; i < sizeof(struct superblock); i++) {
        ((uint8_t *)&g_sb)[i] = s_sector[i];
    }

    if (g_sb.magic == FS_MAGIC) {
        return;
    }

    kprintf("[fs] formatting...\n");
    g_sb.magic = FS_MAGIC;
    for (int i = 0; i < 256; i++) g_sb.bitmap[i] = 0;

    /* 标记 0 ~ 23 扇区为已用（Superblock + Inode区） */
    for (int i = 0; i < DATA_START_LBA; i++) set_bitmap(i, 1);

    write_superblock();

    /* 清空 Inode 表 */
    for (uint32_t i = 0; i < MAX_INODES; i++) {
        for (int j = 0; j < 512; j++) s_sector[j] = 0;
        ata_write_sector(INODE_START_LBA + i, s_sector);
    }

    struct inode root;
    root.used = 1;
    root.type = INODE_TYPE_DIR;
    root.size = 0;
    root.parent_inode = 0;
    str_copy(root.name, "/", 24);
    root.extent_count = 0;
    write_inode(ROOT_INODE, &root);

    g_cwd_inode = ROOT_INODE;
    kprintf("[fs] formatted.\n");
}

/* ============================================================
 *  文件读写与操作
 * ============================================================ */
void fs_write(const char *path, const char *text) {
    uint32_t idx = resolve_path(path);
    if (idx == (uint32_t)-1) { kprintf("Not found\n"); return; }

    static struct inode in;
    read_inode(idx, &in);
    if (in.type != INODE_TYPE_FILE) { kprintf("Not a file\n"); return; }

    int len = str_len(text);
    
    /* 释放旧数据 */
    for (uint32_t i = 0; i < in.extent_count; i++) {
        free_sectors(in.extents[i].lba, in.extents[i].length);
    }
    in.extent_count = 0;

    in.size = len;
    if (len > 0) {
        uint32_t sectors_needed = (len + 511) / 512;
        int start_lba = alloc_sectors(sectors_needed);
        
        if (start_lba < 0) { 
            kprintf("Disk full or too fragmented!\n"); 
            in.size = 0;
            write_inode(idx, &in);
            return; 
        }

        in.extents[0].lba = start_lba;
        in.extents[0].length = sectors_needed;
        in.extent_count = 1;

        /* 写入数据 */
        uint32_t written = 0;
        for (uint32_t i = 0; i < sectors_needed; i++) {
            for (int j = 0; j < 512; j++) s_sector[j] = 0;
            for (int j = 0; j < 512 && (i * 512 + j) < len; j++) {
                s_sector[j] = text[i * 512 + j];
            }
            ata_write_sector(start_lba + i, s_sector);
        }
    }

    write_inode(idx, &in);
    kprintf("Wrote %d bytes to %s\n", len, path);
}

void fs_cat(const char *path) {
    uint32_t idx = resolve_path(path);
    if (idx == (uint32_t)-1) { kprintf("Not found\n"); return; }

    static struct inode in;
    read_inode(idx, &in);
    if (in.type != INODE_TYPE_FILE) { kprintf("Not a file\n"); return; }
    if (in.size == 0) { kprintf("(empty)\n"); return; }

    uint32_t read_bytes = 0;
    for (uint32_t i = 0; i < in.extent_count; i++) {
        for (uint32_t j = 0; j < in.extents[i].length; j++) {
            ata_read_sector(in.extents[i].lba + j, s_sector);
            for (int k = 0; k < 512 && read_bytes < in.size; k++) {
                putchar(s_sector[k]);
                read_bytes++;
            }
        }
    }
    putchar('\n');
}

void fs_touch(const char *path) {
    if (resolve_path(path) != (uint32_t)-1) { kprintf("Exists\n"); return; }
    
    const char *last_slash = path;
    for (const char *p = path; *p; p++) if (*p == '/') last_slash = p;
    int n = last_slash - path;
    if (n == 0) n = 1;
    for (int i = 0; i < n && i < 127; i++) s_parent[i] = path[i];
    s_parent[n] = 0;
    str_copy(s_name, last_slash + 1, 24);

    uint32_t parent_idx = resolve_path(s_parent);
    if (parent_idx == (uint32_t)-1) { kprintf("Parent not found\n"); return; }

    uint32_t new_idx;
    if (alloc_inode(&new_idx) < 0) { kprintf("No free inodes\n"); return; }

    static struct inode in;
    in.used = 1;
    in.type = INODE_TYPE_FILE;
    in.size = 0;
    in.parent_inode = parent_idx;
    str_copy(in.name, s_name, 24);
    in.extent_count = 0;
    write_inode(new_idx, &in);

    read_inode(parent_idx, &in);
    dir_add_entry(&in, s_name, new_idx, INODE_TYPE_FILE);
    write_inode(parent_idx, &in);
    kprintf("Created: %s\n", path);
}

void fs_mkdir(const char *path) {
    if (resolve_path(path) != (uint32_t)-1) { kprintf("Exists\n"); return; }

    const char *last_slash = path;
    for (const char *p = path; *p; p++) if (*p == '/') last_slash = p;
    int n = last_slash - path;
    if (n == 0) n = 1;
    for (int i = 0; i < n && i < 127; i++) s_parent[i] = path[i];
    s_parent[n] = 0;
    str_copy(s_name, last_slash + 1, 24);

    uint32_t parent_idx = resolve_path(s_parent);
    if (parent_idx == (uint32_t)-1) { kprintf("Parent not found\n"); return; }

    uint32_t new_idx;
    if (alloc_inode(&new_idx) < 0) { kprintf("No free inodes\n"); return; }

    static struct inode in;
    in.used = 1;
    in.type = INODE_TYPE_DIR;
    in.size = 0;
    in.parent_inode = parent_idx;
    str_copy(in.name, s_name, 24);
    in.extent_count = 0;
    write_inode(new_idx, &in);

    read_inode(parent_idx, &in);
    dir_add_entry(&in, s_name, new_idx, INODE_TYPE_DIR);
    write_inode(parent_idx, &in);
    kprintf("Created dir: %s\n", path);
}

void fs_rm(const char *path) {
    uint32_t idx = resolve_path(path);
    if (idx == (uint32_t)-1) { kprintf("Not found: %s\n", path); return; }
    if (idx == ROOT_INODE) { kprintf("Cannot remove root\n"); return; }

    static struct inode in;
    read_inode(idx, &in);

    if (in.type == INODE_TYPE_DIR && in.size > 0) {
        kprintf("Directory not empty\n");
        return;
    }

    /* 1. 释放数据区占用的扇区 */
    for (uint32_t i = 0; i < in.extent_count; i++) {
        free_sectors(in.extents[i].lba, in.extents[i].length);
    }

    /* 2. 从父目录中移除目录项 */
    static struct inode parent;
    if (read_inode(in.parent_inode, &parent) == 0) {
        if (parent.extent_count > 0) {
            for (uint32_t i = 0; i < parent.size; i++) {
                uint32_t sector_idx = i / ENTRIES_PER_SECTOR;
                uint32_t sector_off = (i % ENTRIES_PER_SECTOR) * sizeof(struct dir_entry);

                if (sector_idx >= parent.extents[0].length) break;

                uint32_t lba = parent.extents[0].lba + sector_idx;
                if (ata_read_sector(lba, s_sector) < 0) continue;

                struct dir_entry *e = (struct dir_entry *)(s_sector + sector_off);
                if (e->inode_idx == idx) {
                    /* 彻底清空这条目录项 */
                    for (int j = 0; j < 24; j++) e->name[j] = 0;
                    e->inode_idx = 0;
                    e->type = 0;
                    ata_write_sector(lba, s_sector);
                    break;
                }
            }
        }
    }

    /* 3. 回收 Inode */
    in.used = 0;
    in.type = 0;
    in.size = 0;
    in.extent_count = 0;
    for (int i = 0; i < 24; i++) in.name[i] = 0;
    write_inode(idx, &in);

    kprintf("Removed: %s\n", path);
}

void fs_ls(void) {
    static struct inode dir;
    read_inode(g_cwd_inode, &dir);
    if (dir.type != INODE_TYPE_DIR) return;
    kprintf("Contents of %s:\n", g_cwd_path);
    if (dir.size == 0) { kprintf("  (empty)\n"); return; }

    for (uint32_t i = 0; i < dir.size; i++) {
        uint32_t sector_idx = i / ENTRIES_PER_SECTOR;
        uint32_t sector_off = (i % ENTRIES_PER_SECTOR) * sizeof(struct dir_entry);
        uint32_t lba = dir.extents[0].lba + sector_idx;
        ata_read_sector(lba, s_sector);
        struct dir_entry *e = (struct dir_entry *)(s_sector + sector_off);
        
        if (e->inode_idx == 0) continue; /* 已删除 */

        if (e->type == INODE_TYPE_DIR) kprintf("  [DIR]  %s/\n", e->name);
        else {
            static struct inode fin;
            read_inode(e->inode_idx, &fin);
            kprintf("  [FILE] %s  (%u bytes)\n", e->name, fin.size);
        }
    }
}

void fs_cd(const char *path) {
    uint32_t idx = resolve_path(path);
    if (idx == (uint32_t)-1) { kprintf("Not found\n"); return; }
    static struct inode in;
    read_inode(idx, &in);
    if (in.type != INODE_TYPE_DIR) { kprintf("Not a dir\n"); return; }
    
    g_cwd_inode = idx;
    if (path[0] == '/') str_copy(g_cwd_path, path, 128);
    else {
        int n = str_len(g_cwd_path);
        g_cwd_path[n] = '/'; str_copy(g_cwd_path+n+1, path, 128-n-1);
    }
    kprintf("cwd = %s\n", g_cwd_path);
}

void fs_pwd(void) {
    kprintf("%s\n", g_cwd_path);
}

void fs_mv(const char *old_path, const char *new_name) {
    uint32_t idx = resolve_path(old_path);
    if (idx == (uint32_t)-1) { 
        kprintf("Not found: %s\n", old_path); 
        return; 
    }
    if (idx == ROOT_INODE) { 
        kprintf("Cannot rename root\n"); 
        return; 
    }
    
    if (str_len(new_name) >= 24) { 
        kprintf("New name too long (max 23 chars)\n"); 
        return; 
    }

    /* 检查新名字在同一个目录下是否已经存在 */
    static struct inode in;
    read_inode(idx, &in);

    static struct inode parent;
    if (read_inode(in.parent_inode, &parent) < 0) return;

    /* 遍历父目录，检查同名冲突，并找到旧名字的目录项 */
    if (parent.extent_count > 0) {
        for (uint32_t i = 0; i < parent.size; i++) {
            uint32_t sector_idx = i / ENTRIES_PER_SECTOR;
            uint32_t sector_off = (i % ENTRIES_PER_SECTOR) * sizeof(struct dir_entry);
            if (sector_idx >= parent.extents[0].length) break;

            uint32_t lba = parent.extents[0].lba + sector_idx;
            if (ata_read_sector(lba, s_sector) < 0) continue;

            struct dir_entry *e = (struct dir_entry *)(s_sector + sector_off);
            if (e->inode_idx == 0) continue;

            if (str_eq(e->name, new_name)) {
                kprintf("Target already exists: %s\n", new_name);
                return;
            }

            if (e->inode_idx == idx) {
                /* 找到了旧名字的目录项，更新它 */
                str_copy(e->name, new_name, 24);
                if (ata_write_sector(lba, s_sector) < 0) return;
            }
        }
    }

    /* 更新 Inode 自己的名字 */
    str_copy(in.name, new_name, 24);
    write_inode(idx, &in);

    kprintf("Renamed: %s -> %s\n", old_path, new_name);
}