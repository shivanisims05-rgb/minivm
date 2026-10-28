#include "common.h"
#include <stdarg.h>

const char *g_file = "<input>";

void compile_error(int line, const char *fmt, ...) {
    va_list ap;
    fprintf(stderr, "%s:%d: error: ", g_file, line);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(1);
}

void *xcalloc(size_t n, size_t size) {
    void *p = calloc(n, size);
    if (!p) { fprintf(stderr, "out of memory\n"); exit(1); }
    return p;
}

void *xrealloc(void *p, size_t size) {
    p = realloc(p, size);
    if (!p) { fprintf(stderr, "out of memory\n"); exit(1); }
    return p;
}

char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    long n;
    char *buf;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = xcalloc((size_t)n + 1, 1);
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(buf); return NULL; }
    fclose(f);
    return buf;
}
