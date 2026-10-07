typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef unsigned long  uint64_t;

extern void kprintf(const char *fmt, ...);
extern void putchar(char c);

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
#define ATA_ERROR       0x1F1
#define ATA_SECCOUNT    0x1F2
#define ATA_LBA_LOW     0x1F3
#define ATA_LBA_MID     0x1F4
#define ATA_LBA_HIGH    0x1F5
#define ATA_DRIVE_HEAD  0x1F6
#define ATA_STATUS      0x1F7
#define ATA_COMMAND     0x1F7
#define ATA_ALT_STATUS  0x3F6

#define ATA_SR_BSY      0x80
#define ATA_SR_DRDY     0x40
#define ATA_SR_DF       0x20
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

    uint8_t mid  = inb(ATA_LBA_MID);
    uint8_t high = inb(ATA_LBA_HIGH);
    if (mid != 0 || high != 0) return -1;

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
    for (int i = 0; i < 256; i++) {
        *p++ = inw(ATA_DATA);
    }
    ata_delay();
    return 0;
}

static int ata_write_sector(uint32_t lba, const void *buf) {
    if (ata_wait_bsy() < 0) return -1;

    ata_setup_lba(lba, 1);
    outb(ATA_COMMAND, ATA_CMD_WRITE);

    if (ata_wait_drq() < 0) return -1;

    const uint16_t *p = (const uint16_t *)buf;
    for (int i = 0; i < 256; i++) {
        outw(ATA_DATA, *p++);
    }
    ata_delay();

    outb(ATA_COMMAND, ATA_CMD_FLUSH);
    ata_wait_bsy();

    return 0;
}

#define FS_MAGIC        0x4F534653
#define FS_SUPER_LBA    0
#define FS_TABLE_LBA    1
#define FS_DATA_LBA     2

#define MAX_FILES       16
#define NAME_LEN        20
#define SECTORS_PER_FILE 4
#define MAX_FILE_SIZE   (SECTORS_PER_FILE * 512)

struct superblock {
    uint32_t magic;
    uint32_t reserved[127];
} __attribute__((packed));

struct fs_entry {
    char     name[NAME_LEN];
    uint32_t size;
    uint32_t start_lba;
    uint32_t used;
} __attribute__((packed));           /* 32 字节 */

static struct fs_entry g_entries[MAX_FILES];
static uint8_t         sector_buf[512];

static int str_eq(const char *a, const char *b) {
    while (*a && *b) { if (*a != *b) return 0; a++; b++; }
    return *a == *b;
}

static void str_copy(char *dst, const char *src, int max) {
    int i = 0;
    while (src[i] && i < max - 1) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

static int str_len(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

static int write_super(void) {
    for (int i = 0; i < 512; i++) sector_buf[i] = 0;
    struct superblock *sb = (struct superblock *)sector_buf;
    sb->magic = FS_MAGIC;
    return ata_write_sector(FS_SUPER_LBA, sector_buf);
}

static int read_super(void) {
    if (ata_read_sector(FS_SUPER_LBA, sector_buf) < 0) return -1;
    struct superblock *sb = (struct superblock *)sector_buf;
    return sb->magic == FS_MAGIC ? 0 : -1;
}

static int write_table(void) {
    for (int i = 0; i < 512; i++) sector_buf[i] = 0;
    struct fs_entry *tbl = (struct fs_entry *)sector_buf;
    for (int i = 0; i < MAX_FILES; i++) tbl[i] = g_entries[i];
    return ata_write_sector(FS_TABLE_LBA, sector_buf);
}

static int read_table(void) {
    if (ata_read_sector(FS_TABLE_LBA, sector_buf) < 0) return -1;
    struct fs_entry *tbl = (struct fs_entry *)sector_buf;
    for (int i = 0; i < MAX_FILES; i++) g_entries[i] = tbl[i];
    return 0;
}

void fs_init(void) {
    if (ata_init() != 0) {
        kprintf("[fs] ERROR: ATA init failed (no disk?)\n");
        kprintf("[fs] FS disabled\n");
        return;
    }
    kprintf("[fs] ATA disk OK\n");

    if (read_super() == 0 && read_table() == 0) {
        kprintf("[fs] loaded from disk\n");
        return;
    }

    kprintf("[fs] formatting...\n");

    for (int i = 0; i < MAX_FILES; i++) {
        for (int j = 0; j < NAME_LEN; j++) g_entries[i].name[j] = 0;
        g_entries[i].size      = 0;
        g_entries[i].start_lba = FS_DATA_LBA + (uint32_t)i * SECTORS_PER_FILE;
        g_entries[i].used      = 0;
    }

    if (write_super() < 0 || write_table() < 0) {
        kprintf("[fs] format failed\n");
        return;
    }
    kprintf("[fs] formatted\n");
}

static struct fs_entry *fs_find(const char *name) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (g_entries[i].used && str_eq(g_entries[i].name, name)) {
            return &g_entries[i];
        }
    }
    return 0;
}

static struct fs_entry *fs_find_free(void) {
    for (int i = 0; i < MAX_FILES; i++) {
        if (!g_entries[i].used) return &g_entries[i];
    }
    return 0;
}

void fs_touch(const char *name) {
    if (name[0] == 0) {
        kprintf("Usage: touch NAME\n");
        return;
    }
    if (str_len(name) >= NAME_LEN) {
        kprintf("Name too long (max %d)\n", NAME_LEN - 1);
        return;
    }
    if (fs_find(name)) {
        kprintf("File already exists: %s\n", name);
        return;
    }

    struct fs_entry *e = fs_find_free();
    if (!e) {
        kprintf("No free file slots\n");
        return;
    }

    str_copy(e->name, name, NAME_LEN);
    e->size = 0;
    e->used = 1;

    if (write_table() < 0) {
        kprintf("ATA write failed\n");
        e->used = 0;
        return;
    }
    kprintf("Created: %s\n", name);
}

void fs_ls(void) {
    int count = 0;
    kprintf("Files:\n");
    for (int i = 0; i < MAX_FILES; i++) {
        if (g_entries[i].used) {
            kprintf("  %s  (%u bytes, LBA %u)\n",
                    g_entries[i].name,
                    g_entries[i].size,
                    g_entries[i].start_lba);
            count++;
        }
    }
    if (count == 0) kprintf("  (none)\n");
}

void fs_write(const char *name, const char *text) {
    struct fs_entry *e = fs_find(name);
    if (!e) {
        kprintf("File not found: %s\n", name);
        return;
    }

    int len = str_len(text);
    if (len > MAX_FILE_SIZE - 1) {
        len = MAX_FILE_SIZE - 1;
        kprintf("Warning: truncated to %d bytes\n", len);
    }

    uint32_t written = 0;
    uint32_t lba = e->start_lba;

    while (written < (uint32_t)len) {
        for (int i = 0; i < 512; i++) sector_buf[i] = 0;

        int chunk = len - written;
        if (chunk > 512) chunk = 512;

        for (int i = 0; i < chunk; i++) {
            sector_buf[i] = (uint8_t)text[written + i];
        }

        if (ata_write_sector(lba, sector_buf) < 0) {
            kprintf("ATA write failed\n");
            return;
        }

        written += chunk;
        lba++;
    }

    e->size = (uint32_t)len;

    if (write_table() < 0) {
        kprintf("ATA write failed\n");
        return;
    }
    kprintf("Wrote %u bytes to %s\n", (unsigned int)len, name);
}

void fs_cat(const char *name) {
    struct fs_entry *e = fs_find(name);
    if (!e) {
        kprintf("File not found: %s\n", name);
        return;
    }
    if (e->size == 0) {
        kprintf("(empty)\n");
        return;
    }

    uint32_t remaining = e->size;
    uint32_t lba = e->start_lba;

    while (remaining > 0) {
        if (ata_read_sector(lba, sector_buf) < 0) {
            kprintf("ATA read failed\n");
            return;
        }

        int chunk = remaining > 512 ? 512 : (int)remaining;
        for (int i = 0; i < chunk; i++) {
            putchar((char)sector_buf[i]);
        }

        remaining -= chunk;
        lba++;
    }
    putchar('\n');
}

void fs_rm(const char *name) {
    struct fs_entry *e = fs_find(name);
    if (!e) {
        kprintf("File not found: %s\n", name);
        return;
    }

    e->used = 0;
    e->size = 0;
    for (int i = 0; i < NAME_LEN; i++) e->name[i] = 0;

    if (write_table() < 0) {
        kprintf("ATA write failed\n");
        return;
    }
    kprintf("Removed: %s\n", name);
}

void fs_mv(const char *old_name, const char *new_name) {
    struct fs_entry *e = fs_find(old_name);
    if (!e) {
        kprintf("File not found: %s\n", old_name);
        return;
    }
    if (str_len(new_name) >= NAME_LEN) {
        kprintf("Name too long (max %d)\n", NAME_LEN - 1);
        return;
    }
    if (fs_find(new_name)) {
        kprintf("Target already exists: %s\n", new_name);
        return;
    }

    str_copy(e->name, new_name, NAME_LEN);

    if (write_table() < 0) {
        kprintf("ATA write failed\n");
        return;
    }
    kprintf("Renamed: %s -> %s\n", old_name, new_name);
}