/* interp.c - tree-walking interpreter.
 *
 * It executes the same analysed AST that the code generator uses, but instead
 * of emitting bytecode it evaluates the tree recursively. It exists for two
 * reasons: (1) a baseline for the speed comparison, and (2) a second,
 * independent implementation of the language semantics, so the test-suite can
 * check that the compiler + VM produce exactly the same output.
 */
#include "interp.h"
#include <setjmp.h>

#define CALL_MAX 5000

typedef struct {
    const Program *prog;
    FILE *out;
    jmp_buf jb;
    char err[160];
    int depth;
    int retval;
    long calls;
} Ctx;

static void fail(Ctx *c, const char *msg) {
    snprintf(c->err, sizeof c->err, "%s", msg);
    longjmp(c->jb, 1);
}

static int call_func(Ctx *c, int fi, const int *args);

static int eval(Ctx *c, int *loc, const Node *n) {
    int a, b, i;
    switch (n->kind) {
    case N_NUM: return n->value;
    case N_VAR: return loc[n->slot];
    case N_NEG: return w_sub(0, eval(c, loc, n->a));
    case N_NOT: return eval(c, loc, n->a) == 0;
    case N_AND: return eval(c, loc, n->a) != 0 && eval(c, loc, n->b) != 0;
    case N_OR: return eval(c, loc, n->a) != 0 || eval(c, loc, n->b) != 0;
    case N_CALL: {
        int args[n->n + 1];
        for (i = 0; i < n->n; i++) args[i] = eval(c, loc, n->list[i]);
        return call_func(c, n->func, args);
    }
    case N_BIN:
        /* > and <= are compiled as swapped < , so they evaluate right-to-left;
         * mirror that here so both back ends agree even with side effects. */
        if (n->op == B_GT || n->op == B_LE) {
            b = eval(c, loc, n->b);
            a = eval(c, loc, n->a);
        } else {
            a = eval(c, loc, n->a);
            b = eval(c, loc, n->b);
        }
        switch (n->op) {
        case B_ADD: return w_add(a, b);
        case B_SUB: return w_sub(a, b);
        case B_MUL: return w_mul(a, b);
        case B_DIV: { int res; if (w_div(a, b, &res)) fail(c, "division by zero"); return res; }
        case B_MOD: { int res; if (w_mod(a, b, &res)) fail(c, "division by zero (in %)"); return res; }
        case B_LT: return a < b;
        case B_GT: return a > b;
        case B_LE: return a <= b;
        case B_GE: return a >= b;
        case B_EQ: return a == b;
        case B_NE: return a != b;
        }
        return 0;
    default: return 0;
    }
}

/* returns 0 normally, 1 after "return" (value in c->retval), 2 after "break", 3 after "continue" */
static int exec(Ctx *c, int *loc, const Node *n) {
    int i;
    switch (n->kind) {
    case N_BLOCK:
        for (i = 0; i < n->n; i++) {
            int r = exec(c, loc, n->list[i]);
            if (r) return r;
        }
        return 0;
    case N_DECL: loc[n->slot] = n->a ? eval(c, loc, n->a) : 0; return 0;
    case N_ASSIGN: loc[n->slot] = eval(c, loc, n->a); return 0;
    case N_IF:
        if (eval(c, loc, n->a) != 0) return exec(c, loc, n->b);
        return n->c ? exec(c, loc, n->c) : 0;
    case N_WHILE:
        while (eval(c, loc, n->a) != 0) {
            int r = exec(c, loc, n->b);
            if (r == 1) return 1;
            if (r == 2) break;
            if (n->c) exec(c, loc, n->c);   /* the step of a for loop; also runs after "continue" */
        }
        return 0;
    case N_BREAK: return 2;
    case N_CONTINUE: return 3;
    case N_RETURN: {
        int v = eval(c, loc, n->a);
        c->retval = v;
        return 1;
    }
    case N_PRINT: {
        int v = eval(c, loc, n->a);
        if (c->out) fprintf(c->out, "%d\n", v);
        return 0;
    }
    case N_EXPR: eval(c, loc, n->a); return 0;
    default: return 0;
    }
}

static int call_func(Ctx *c, int fi, const int *args) {
    const Node *f = c->prog->funcs[fi];
    int loc[f->nlocals + 1];
    int i, returned, v;
    if (c->depth >= CALL_MAX) fail(c, "call stack overflow (recursion too deep)");
    memset(loc, 0, sizeof loc);
    for (i = 0; i < f->nparams; i++) loc[i] = args[i];
    c->depth++;
    c->calls++;
    c->retval = 0;
    returned = exec(c, loc, f->a) == 1;
    v = returned ? c->retval : 0;
    c->depth--;
    return v;
}

int interp_run(const Program *prog, FILE *out, InterpResult *r) {
    Ctx c;
    const int main_idx = prog->main_idx;
    memset(&c, 0, sizeof c);
    memset(r, 0, sizeof *r);
    c.prog = prog;
    c.out = out;
    if (setjmp(c.jb)) {
        snprintf(r->err, sizeof r->err, "%s", c.err);
        r->calls = c.calls;
        return 1;
    }
    r->exit_value = call_func(&c, main_idx, NULL);
    r->calls = c.calls;
    return 0;
}
