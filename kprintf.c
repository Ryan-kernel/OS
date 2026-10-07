#include "kprintf.h"

/* ============================================================
 *  可变参数支持（GCC 内建，不依赖 <stdarg.h>）
 * ============================================================ */
typedef __builtin_va_list va_list;
#define va_start(ap, last)  __builtin_va_start(ap, last)
#define va_arg(ap, type)    __builtin_va_arg(ap, type)
#define va_end(ap)          __builtin_va_end(ap)

/* ============================================================
 *  外部依赖（由 kernel.c 提供）
 * ============================================================ */
extern void putchar(char c);
extern void print(const char *s);

/* ============================================================
 *  数字 → 字符串
 * ============================================================ */

/* 无符号整数转字符串
 *   value  要转换的值
 *   buf    输出缓冲区（至少 24 字节）
 *   base   进制（10 或 16）
 *   upper  十六进制是否大写（仅 base==16 有效）
 *   返回   写入的字符数（不含结尾 '\0'）
 */
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

/* 有符号整数转字符串
 * 处理 INT64_MIN 溢出问题：用 -(value+1)+1 的技巧
 */
static int itoa_signed(int64_t value, char *buf) {
    if (value < 0) {
        buf[0] = '-';
        uint64_t uv = (uint64_t)(-(value + 1)) + 1ULL;
        return 1 + utoa_base(uv, buf + 1, 10, 0);
    }
    return utoa_base((uint64_t)value, buf, 10, 0);
}

/* ============================================================
 *  kprintf 主体
 * ============================================================
 *  支持的格式符：
 *    %c   字符（char 会被提升为 int，按 int 取）
 *    %s   字符串（NULL 输出 "(null)"）
 *    %d   有符号十进制（按 int 取，32 位）
 *    %u   无符号十进制（按 unsigned int 取，32 位）
 *    %x   十六进制小写（按 unsigned int 取，32 位）
 *    %X   十六进制大写（按 unsigned int 取，32 位）
 *    %p   指针（按 void* 取，64 位，输出 "0x..."）
 *    %ld  有符号十进制（按 long 取，64 位）
 *    %lu  无符号十进制（按 unsigned long 取，64 位）
 *    %lx  十六进制小写（按 unsigned long 取，64 位）
 *    %lX  十六进制大写（按 unsigned long 取，64 位）
 *    %%   百分号本身
 *
 *  不支持的格式符会原样输出 "%X"。
 * ============================================================ */
void kprintf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    char buf[32];

    while (*fmt) {
        if (*fmt != '%') {
            putchar(*fmt++);
            continue;
        }

        fmt++;

        if (*fmt == 0) {
            putchar('%');
            break;
        }

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

        switch (*fmt) {
            case 'c': {
                int c = va_arg(ap, int);
                putchar((char)c);
                break;
            }

            case 's': {
                const char *s = va_arg(ap, const char *);
                if (!s) s = "(null)";
                print(s);
                break;
            }

            case 'd': {
                if (is_long) {
                    long v = va_arg(ap, long);
                    itoa_signed((int64_t)v, buf);
                } else {
                    int v = va_arg(ap, int);
                    itoa_signed((int64_t)v, buf);
                }
                print(buf);
                break;
            }

            case 'u': {
                if (is_long) {
                    unsigned long v = va_arg(ap, unsigned long);
                    utoa_base((uint64_t)v, buf, 10, 0);
                } else {
                    unsigned int v = va_arg(ap, unsigned int);
                    utoa_base((uint64_t)v, buf, 10, 0);
                }
                print(buf);
                break;
            }

            case 'x': {
                if (is_long) {
                    unsigned long v = va_arg(ap, unsigned long);
                    utoa_base((uint64_t)v, buf, 16, 0);
                } else {
                    unsigned int v = va_arg(ap, unsigned int);
                    utoa_base((uint64_t)v, buf, 16, 0);
                }
                print(buf);
                break;
            }

            case 'X': {
                if (is_long) {
                    unsigned long v = va_arg(ap, unsigned long);
                    utoa_base((uint64_t)v, buf, 16, 1);
                } else {
                    unsigned int v = va_arg(ap, unsigned int);
                    utoa_base((uint64_t)v, buf, 16, 1);
                }
                print(buf);
                break;
            }

            case 'p': {
                void *v = va_arg(ap, void *);
                print("0x");
                utoa_base((uint64_t)v, buf, 16, 0);
                print(buf);
                break;
            }

            case '%': {
                putchar('%');
                break;
            }

            default: {
                putchar('%');
                if (is_long) putchar('l');
                putchar(*fmt);
                break;
            }
        }

        fmt++;
    }

    va_end(ap);
}