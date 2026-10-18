#include "kprintf.h"

/* ============================================================
 *  可变参数支持
 * ============================================================ */
typedef __builtin_va_list va_list;
#define va_start(ap, last)  __builtin_va_start(ap, last)
#define va_arg(ap, type)    __builtin_va_arg(ap, type)
#define va_end(ap)          __builtin_va_end(ap)

extern void putchar(char c);
extern void print(const char *s);

/* ============================================================
 *  数字 → 字符串
 * ============================================================ */
static int utoa_base(uint64_t value, char *buf, int base, int upper) {
    char tmp[24];
    int i = 0;

    if (value == 0) {
        buf[0] = '0';
        buf[1] = 0;
        return 1;
    }

    const char *digits = upper
        ? "0123456789ABCDEF"
        : "0123456789abcdef";

    while (value > 0) {
        tmp[i++] = digits[value % (uint64_t)base];
        value /= (uint64_t)base;
    }

    int len = i;
    int j = 0;
    while (i > 0) {
        buf[j++] = tmp[--i];
    }
    buf[j] = 0;
    return len;
}

static int itoa_signed(int64_t value, char *buf) {
    if (value < 0) {
        buf[0] = '-';
        uint64_t uv = (uint64_t)(-(value + 1)) + 1ULL;
        return 1 + utoa_base(uv, buf + 1, 10, 0);
    }
    return utoa_base((uint64_t)value, buf, 10, 0);
}

/* ============================================================
 *  带宽度/对齐的输出辅助
 * ============================================================ */

/* 输出字符串，指定最小宽度、左对齐、补 0 */
static void out_str_padded(const char *s, int width, int left_align, int pad_zero) {
    if (!s) s = "(null)";

    int len = 0;
    while (s[len]) len++;

    int pad = (width > len) ? (width - len) : 0;

    if (left_align) {
        /* 左对齐：先输出字符串，再补空格 */
        while (*s) putchar(*s++);
        while (pad-- > 0) putchar(' ');
    } else {
        /* 右对齐：先补，再输出 */
        char padc = pad_zero ? '0' : ' ';
        while (pad-- > 0) putchar(padc);
        while (*s) putchar(*s++);
    }
}

/* ============================================================
 *  kprintf 主体
 * ============================================================ */
void kprintf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    char num_buf[32];

    while (*fmt) {
        /* 普通字符 */
        if (*fmt != '%') {
            putchar(*fmt++);
            continue;
        }

        fmt++;   /* 跳过 '%' */

        if (*fmt == 0) {
            putchar('%');
            break;
        }

        /* ---------- 解析 flags ---------- */
        int left_align = 0;
        int pad_zero   = 0;

        while (*fmt == '-' || *fmt == '0') {
            if (*fmt == '-') left_align = 1;
            if (*fmt == '0') pad_zero = 1;
            fmt++;
        }

        /* ---------- 解析 width ---------- */
        int width = 0;
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }

        /* ---------- 解析长度修饰符 'l' ---------- */
        int is_long = 0;
        if (*fmt == 'l') {
            is_long = 1;
            fmt++;
            if (*fmt == 0) {
                putchar('%');
                putchar('l');
                break;
            }
        }

        /* ---------- 处理类型 ---------- */
        switch (*fmt) {
            case 'c': {
                int c = va_arg(ap, int);
                /* 宽度 1 或 0，直接输出 */
                if (width <= 1) {
                    putchar((char)c);
                } else {
                    /* 右对齐补空格（字符一般不补 0） */
                    int pad = width - 1;
                    if (!left_align) {
                        while (pad-- > 0) putchar(' ');
                    }
                    putchar((char)c);
                    if (left_align) {
                        while (pad-- > 0) putchar(' ');
                    }
                }
                break;
            }

            case 's': {
                const char *s = va_arg(ap, const char *);
                out_str_padded(s, width, left_align, pad_zero);
                break;
            }

            case 'd': {
                if (is_long) {
                    long v = va_arg(ap, long);
                    itoa_signed((int64_t)v, num_buf);
                } else {
                    int v = va_arg(ap, int);
                    itoa_signed((int64_t)v, num_buf);
                }
                out_str_padded(num_buf, width, left_align, pad_zero);
                break;
            }

            case 'u': {
                if (is_long) {
                    unsigned long v = va_arg(ap, unsigned long);
                    utoa_base((uint64_t)v, num_buf, 10, 0);
                } else {
                    unsigned int v = va_arg(ap, unsigned int);
                    utoa_base((uint64_t)v, num_buf, 10, 0);
                }
                out_str_padded(num_buf, width, left_align, pad_zero);
                break;
            }

            case 'x': {
                if (is_long) {
                    unsigned long v = va_arg(ap, unsigned long);
                    utoa_base((uint64_t)v, num_buf, 16, 0);
                } else {
                    unsigned int v = va_arg(ap, unsigned int);
                    utoa_base((uint64_t)v, num_buf, 16, 0);
                }
                out_str_padded(num_buf, width, left_align, pad_zero);
                break;
            }

            case 'X': {
                if (is_long) {
                    unsigned long v = va_arg(ap, unsigned long);
                    utoa_base((uint64_t)v, num_buf, 16, 1);
                } else {
                    unsigned int v = va_arg(ap, unsigned int);
                    utoa_base((uint64_t)v, num_buf, 16, 1);
                }
                out_str_padded(num_buf, width, left_align, pad_zero);
                break;
            }

            case 'p': {
                void *v = va_arg(ap, void *);
                char hexbuf[20];
                utoa_base((uint64_t)v, hexbuf, 16, 0);
                int hlen = 0;
                while (hexbuf[hlen]) hlen++;
                int total = 2 + hlen;

                if (width > total) {
                    int pad = width - total;
                    if (!left_align) {
                        if (pad_zero) {
                            putchar('0'); putchar('x');
                            int hexpad = pad;
                            while (hexpad-- > 0) putchar('0');
                        } else {
                            while (pad-- > 0) putchar(' ');
                            putchar('0'); putchar('x');
                        }
                    } else {
                        putchar('0'); putchar('x');
                        print(hexbuf);
                        while (pad-- > 0) putchar(' ');
                        break;
                    }
                } else {
                    putchar('0'); putchar('x');
                }
                print(hexbuf);
                break;
            }

            case '%': {
                putchar('%');
                break;
            }

            default: {
                putchar('%');
                if (left_align) putchar('-');
                if (pad_zero) putchar('0');
                if (is_long) putchar('l');
                putchar(*fmt);
                break;
            }
        }

        fmt++;
    }

    va_end(ap);
}