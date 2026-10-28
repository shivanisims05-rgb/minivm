/* common.h - shared helpers: error reporting, memory, 32-bit wrap-around math */
#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_NAME 64

#if defined(__GNUC__)
#define NORETURN __attribute__((noreturn))
#else
#define NORETURN
#endif

extern const char *g_file; /* name of the source file, used in error messages */

/* Report a compile-time error (lexer, parser or semantic) and exit(1). */
NORETURN void compile_error(int line, const char *fmt, ...);

void *xcalloc(size_t n, size_t size);
void *xrealloc(void *p, size_t size);
char *read_file(const char *path); /* returns malloc'd text or NULL */

static inline void copy_name(char *dst, const char *src) { snprintf(dst, MAX_NAME, "%s", src); }

/* Arithmetic used by the VM, the tree-walking interpreter and constant
 * folding. All three MUST behave identically, so it lives in one place.
 * Integers are 32-bit and wrap around on overflow. */
static inline int w_add(int a, int b) { return (int)((unsigned)a + (unsigned)b); }
static inline int w_sub(int a, int b) { return (int)((unsigned)a - (unsigned)b); }
static inline int w_mul(int a, int b) { return (int)((unsigned)a * (unsigned)b); }
/* returns 1 on division by zero, otherwise 0 and stores a / b in *res */
static inline int w_div(int a, int b, int *res) {
    if (b == 0) return 1;
    if (b == -1) { *res = w_sub(0, a); return 0; } /* avoids INT_MIN / -1 trap */
    *res = a / b;
    return 0;
}
/* returns 1 when b is zero, otherwise 0 and stores a % b (sign follows a, as in C) */
static inline int w_mod(int a, int b, int *res) {
    if (b == 0) return 1;
    if (b == -1) { *res = 0; return 0; }
    *res = a % b;
    return 0;
}

#endif
