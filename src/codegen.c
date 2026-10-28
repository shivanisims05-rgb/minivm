/* codegen.c - bytecode generator.
 *
 * Expressions are generated in post-order (operands first, then the operator),
 * which is exactly what a stack machine wants.
 *
 * Jumps use backpatching: when "if" or "while" emits JZ/JMP the target address
 * is not known yet, so a placeholder is emitted and patched once the target is
 * reached. Calls are patched at the very end because a function may be called
 * before it is defined.
 *
 * The VM only has LT and EQ, so the other comparisons are built from them:
 *     a > b   ->  b a LT                (operands evaluated right-to-left)
 *     a <= b  ->  b a LT  PUSH 0 EQ     (not (b < a))
 *     a >= b  ->  a b LT  PUSH 0 EQ     (not (a < b))
 *     a != b  ->  a b EQ  PUSH 0 EQ     (not (a == b))
 *
 * && and || short-circuit with the same JZ/JMP backpatching as "if":
 *     a && b  ->  a JZ Lfalse  b <bool>  JMP Lend  Lfalse: PUSH 0  Lend:
 *     a || b  ->  a JZ Lrhs  PUSH 1  JMP Lend  Lrhs: b <bool>  Lend:
 * where <bool> turns any value into 0 or 1 (PUSH 0 EQ PUSH 0 EQ).
 *
 * break and continue emit a JMP with a blank target and remember where it is;
 * the enclosing loop patches them: continue goes to the loop step (or the test
 * of a plain while), break goes to the first instruction after the loop.
 */
#include "codegen.h"

typedef struct {
    BcProgram *p;
    int *site;      /* code index of each CALL operand that needs patching */
    int *callee;    /* function index each of those calls refers to */
    int ncalls, capcalls;
    int *brk; int nbrk, capbrk;     /* pending break jumps (all open loops) */
    int *cont; int ncont, capcont;  /* pending continue jumps */
} Gen;

static void push_site(int **arr, int *n, int *cap, int at) {
    if (*n == *cap) {
        *cap = *cap ? *cap * 2 : 8;
        *arr = xrealloc(*arr, (size_t)*cap * sizeof(int));
    }
    (*arr)[(*n)++] = at;
}

static int emit(Gen *g, int w) {
    BcProgram *p = g->p;
    if (p->len == p->cap) {
        p->cap *= 2;
        p->code = xrealloc(p->code, (size_t)p->cap * sizeof(int));
    }
    p->code[p->len] = w;
    return p->len++;
}

/* emits "op operand" and returns the index of the operand word (for patching) */
static int emit1(Gen *g, int op, int operand) {
    emit(g, op);
    return emit(g, operand);
}

static void patch_here(Gen *g, int at) { g->p->code[at] = g->p->len; }

static void gen_expr(Gen *g, const Node *n);

static void gen_not(Gen *g) { /* logical not of the value on top: x == 0 */
    emit1(g, OP_PUSH, 0);
    emit(g, OP_EQ);
}

static void gen_bool(Gen *g) { /* x != 0 as 0 or 1 */
    gen_not(g);
    gen_not(g);
}

static void gen_bin(Gen *g, const Node *n) {
    switch (n->op) {
    case B_ADD: gen_expr(g, n->a); gen_expr(g, n->b); emit(g, OP_ADD); break;
    case B_SUB: gen_expr(g, n->a); gen_expr(g, n->b); emit(g, OP_SUB); break;
    case B_MUL: gen_expr(g, n->a); gen_expr(g, n->b); emit(g, OP_MUL); break;
    case B_DIV: gen_expr(g, n->a); gen_expr(g, n->b); emit(g, OP_DIV); break;
    case B_MOD: gen_expr(g, n->a); gen_expr(g, n->b); emit(g, OP_MOD); break;
    case B_LT:  gen_expr(g, n->a); gen_expr(g, n->b); emit(g, OP_LT); break;
    case B_EQ:  gen_expr(g, n->a); gen_expr(g, n->b); emit(g, OP_EQ); break;
    case B_GT:  gen_expr(g, n->b); gen_expr(g, n->a); emit(g, OP_LT); break;
    case B_LE:  gen_expr(g, n->b); gen_expr(g, n->a); emit(g, OP_LT); gen_not(g); break;
    case B_GE:  gen_expr(g, n->a); gen_expr(g, n->b); emit(g, OP_LT); gen_not(g); break;
    case B_NE:  gen_expr(g, n->a); gen_expr(g, n->b); emit(g, OP_EQ); gen_not(g); break;
    }
}

static void gen_expr(Gen *g, const Node *n) {
    int i, at;
    switch (n->kind) {
    case N_NUM: emit1(g, OP_PUSH, n->value); break;
    case N_VAR: emit1(g, OP_LOAD, n->slot); break;
    case N_NEG: emit1(g, OP_PUSH, 0); gen_expr(g, n->a); emit(g, OP_SUB); break;
    case N_NOT: gen_expr(g, n->a); gen_not(g); break;
    case N_AND: {
        int jz, jmp;
        gen_expr(g, n->a);
        jz = emit1(g, OP_JZ, 0);
        gen_expr(g, n->b);
        gen_bool(g);
        jmp = emit1(g, OP_JMP, 0);
        patch_here(g, jz);
        emit1(g, OP_PUSH, 0);
        patch_here(g, jmp);
        break;
    }
    case N_OR: {
        int jz, jmp;
        gen_expr(g, n->a);
        jz = emit1(g, OP_JZ, 0);
        emit1(g, OP_PUSH, 1);
        jmp = emit1(g, OP_JMP, 0);
        patch_here(g, jz);
        gen_expr(g, n->b);
        gen_bool(g);
        patch_here(g, jmp);
        break;
    }
    case N_BIN: gen_bin(g, n); break;
    case N_CALL:
        for (i = 0; i < n->n; i++) gen_expr(g, n->list[i]); /* left to right */
        at = emit1(g, OP_CALL, 0);
        if (g->ncalls == g->capcalls) {
            g->capcalls = g->capcalls ? g->capcalls * 2 : 16;
            g->site = xrealloc(g->site, (size_t)g->capcalls * sizeof(int));
            g->callee = xrealloc(g->callee, (size_t)g->capcalls * sizeof(int));
        }
        g->site[g->ncalls] = at;
        g->callee[g->ncalls] = n->func;
        g->ncalls++;
        break;
    default: break;
    }
}

static void gen_stmt(Gen *g, const Node *n) {
    int i, jz, jmp, top, nb, nc;
    switch (n->kind) {
    case N_BLOCK:
        for (i = 0; i < n->n; i++) gen_stmt(g, n->list[i]);
        break;
    case N_DECL:
        if (n->a) gen_expr(g, n->a); else emit1(g, OP_PUSH, 0);
        emit1(g, OP_STORE, n->slot);
        break;
    case N_ASSIGN:
        gen_expr(g, n->a);
        emit1(g, OP_STORE, n->slot);
        break;
    case N_IF:
        gen_expr(g, n->a);
        jz = emit1(g, OP_JZ, 0);
        gen_stmt(g, n->b);
        if (n->c) {
            jmp = emit1(g, OP_JMP, 0);
            patch_here(g, jz);
            gen_stmt(g, n->c);
            patch_here(g, jmp);
        } else {
            patch_here(g, jz);
        }
        break;
    case N_WHILE:
        top = g->p->len;
        nb = g->nbrk;
        nc = g->ncont;
        gen_expr(g, n->a);
        jz = emit1(g, OP_JZ, 0);
        gen_stmt(g, n->b);
        for (i = nc; i < g->ncont; i++) patch_here(g, g->cont[i]);   /* continue lands here */
        g->ncont = nc;
        if (n->c) gen_stmt(g, n->c);
        emit1(g, OP_JMP, top);
        patch_here(g, jz);
        for (i = nb; i < g->nbrk; i++) patch_here(g, g->brk[i]);     /* break lands after the loop */
        g->nbrk = nb;
        break;
    case N_BREAK:
        push_site(&g->brk, &g->nbrk, &g->capbrk, emit1(g, OP_JMP, 0));
        break;
    case N_CONTINUE:
        push_site(&g->cont, &g->ncont, &g->capcont, emit1(g, OP_JMP, 0));
        break;
    case N_RETURN:
        gen_expr(g, n->a);
        emit(g, OP_RET);
        break;
    case N_PRINT:
        gen_expr(g, n->a);
        emit(g, OP_PRINT);
        break;
    case N_EXPR: /* a call used as a statement: throw the result away */
        gen_expr(g, n->a);
        emit(g, OP_POP);
        break;
    default: break;
    }
}

BcProgram *generate(const Program *prog) {
    Gen g;
    BcProgram *p = xcalloc(1, sizeof *p);
    int i, entry;

    memset(&g, 0, sizeof g);
    g.p = p;
    p->cap = 256;
    p->code = xcalloc((size_t)p->cap, sizeof(int));
    p->nfuncs = prog->nfuncs;
    p->funcs = xcalloc((size_t)prog->nfuncs + 1, sizeof(FuncInfo));

    /* entry stub: CALL main ; HALT */
    entry = emit1(&g, OP_CALL, 0);
    emit(&g, OP_HALT);

    for (i = 0; i < prog->nfuncs; i++) {
        const Node *f = prog->funcs[i];
        copy_name(p->funcs[i].name, f->name);
        p->funcs[i].addr = p->len;
        emit(&g, f->nparams);
        emit(&g, f->nlocals);
        gen_stmt(&g, f->a);
        emit1(&g, OP_PUSH, 0);   /* falling off the end returns 0 */
        emit(&g, OP_RET);
    }

    for (i = 0; i < g.ncalls; i++) p->code[g.site[i]] = p->funcs[g.callee[i]].addr;
    p->code[entry] = p->funcs[prog->main_idx].addr;
    free(g.site);
    free(g.callee);
    free(g.brk);
    free(g.cont);
    return p;
}
