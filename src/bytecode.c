#include "bytecode.h"

static const char *NAMES[OP_COUNT] = {
    "PUSH", "POP", "LOAD", "STORE", "ADD", "SUB", "MUL", "DIV",
    "LT", "EQ", "JMP", "JZ", "CALL", "RET", "PRINT", "HALT", "MOD"
};

const char *op_name(int op) { return (op >= 0 && op < OP_COUNT) ? NAMES[op] : "???"; }

/* ---------- bytecode file ---------- */

int bc_save(const BcProgram *p, const char *path) {
    FILE *f = fopen(path, "wb");
    int i, ok = 1;
    if (!f) return 0;
    ok &= fwrite("MBC1", 1, 4, f) == 4;
    ok &= fwrite(&p->len, sizeof(int), 1, f) == 1;
    ok &= fwrite(p->code, sizeof(int), (size_t)p->len, f) == (size_t)p->len;
    ok &= fwrite(&p->nfuncs, sizeof(int), 1, f) == 1;
    for (i = 0; i < p->nfuncs; i++) {
        ok &= fwrite(p->funcs[i].name, 1, MAX_NAME, f) == MAX_NAME;
        ok &= fwrite(&p->funcs[i].addr, sizeof(int), 1, f) == 1;
    }
    if (fclose(f) != 0) ok = 0;
    return ok;
}

BcProgram *bc_load(const char *path, char *err, size_t errlen) {
    FILE *f = fopen(path, "rb");
    char magic[4];
    BcProgram *p;
    int i, len, nf;
    if (!f) { snprintf(err, errlen, "cannot open '%s'", path); return NULL; }
    if (fread(magic, 1, 4, f) != 4 || memcmp(magic, "MBC1", 4) != 0) {
        snprintf(err, errlen, "'%s' is not a bytecode file (bad magic number)", path);
        fclose(f);
        return NULL;
    }
    if (fread(&len, sizeof(int), 1, f) != 1 || len < 3 || len > 50000000) goto bad;
    p = xcalloc(1, sizeof *p);
    p->len = p->cap = len;
    p->code = xcalloc((size_t)len, sizeof(int));
    if (fread(p->code, sizeof(int), (size_t)len, f) != (size_t)len) goto bad;
    if (fread(&nf, sizeof(int), 1, f) != 1 || nf < 0 || nf > 100000) goto bad;
    p->nfuncs = nf;
    p->funcs = xcalloc((size_t)nf + 1, sizeof(FuncInfo));
    for (i = 0; i < nf; i++) {
        if (fread(p->funcs[i].name, 1, MAX_NAME, f) != MAX_NAME) goto bad;
        p->funcs[i].name[MAX_NAME - 1] = '\0';
        if (fread(&p->funcs[i].addr, sizeof(int), 1, f) != 1) goto bad;
    }
    fclose(f);
    return p;
bad:
    snprintf(err, errlen, "'%s' is truncated or corrupt", path);
    fclose(f);
    return NULL;
}

/* ---------- disassembler ---------- */

static const FuncInfo *func_at(const BcProgram *p, int addr) {
    int i;
    for (i = 0; i < p->nfuncs; i++)
        if (p->funcs[i].addr == addr) return &p->funcs[i];
    return NULL;
}

void disassemble(const BcProgram *p, FILE *out) {
    int i = 0;
    while (i < p->len) {
        const FuncInfo *fi = func_at(p, i);
        int op;
        if (i == 0) fprintf(out, "_start:\n");
        if (fi && i + 1 < p->len) {
            fprintf(out, "\n%s:   (params=%d, locals=%d)\n", fi->name, p->code[i], p->code[i + 1]);
            i += 2;
            continue;
        }
        op = p->code[i];
        if (op < 0 || op >= OP_COUNT) {
            fprintf(out, "%04d  ???    (%d)\n", i, op);
            i++;
        } else if (op_has_operand(op)) {
            int arg = (i + 1 < p->len) ? p->code[i + 1] : 0;
            fprintf(out, "%04d  %-6s %d", i, op_name(op), arg);
            if (op == OP_CALL && func_at(p, arg)) fprintf(out, "      ; %s", func_at(p, arg)->name);
            fputc('\n', out);
            i += 2;
        } else {
            fprintf(out, "%04d  %s\n", i, op_name(op));
            i++;
        }
    }
}
