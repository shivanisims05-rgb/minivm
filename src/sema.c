/* sema.c - semantic analysis.
 *
 * The symbol table is a stack of (name, slot) pairs. Entering a block
 * remembers where the block's symbols start; leaving the block drops them.
 * Slots are never reused inside a function, so every variable gets a unique
 * slot and the function's frame size is simply the number of slots handed out.
 *
 * Checks performed:
 *   - variable declared before use, and not declared twice in one scope
 *   - function defined once, called with the right number of arguments
 *   - program has "int main()" with no parameters
 */
#include "sema.h"

#define MAX_SYMS 4096
#define MAX_DEPTH 256

typedef struct { char name[MAX_NAME]; int slot; } Sym;

static Sym syms[MAX_SYMS];
static int nsyms;
static int bases[MAX_DEPTH];
static int depth;
static int next_slot;
static const Program *G;

static void push_scope(int line) {
    if (depth + 1 >= MAX_DEPTH) compile_error(line, "blocks are nested too deeply");
    bases[++depth] = nsyms;
}
static void pop_scope(void) { nsyms = bases[depth--]; }

static int lookup(const char *name) {
    int i;
    for (i = nsyms - 1; i >= 0; i--)
        if (strcmp(syms[i].name, name) == 0) return syms[i].slot;
    return -1;
}

static int declare(const char *name, int line) {
    int i;
    for (i = bases[depth]; i < nsyms; i++)
        if (strcmp(syms[i].name, name) == 0)
            compile_error(line, "variable '%s' is already declared in this scope", name);
    if (nsyms >= MAX_SYMS) compile_error(line, "too many variables in one function");
    copy_name(syms[nsyms].name, name);
    syms[nsyms].slot = next_slot;
    nsyms++;
    return next_slot++;
}

static int find_func(const char *name) {
    int i;
    for (i = 0; i < G->nfuncs; i++)
        if (strcmp(G->funcs[i]->name, name) == 0) return i;
    return -1;
}

static void sema_stmt(Node *n);

static int loop_depth;

static void sema_expr(Node *n) {
    int i;
    switch (n->kind) {
    case N_NUM: break;
    case N_VAR:
        n->slot = lookup(n->name);
        if (n->slot < 0) compile_error(n->line, "undeclared variable '%s'", n->name);
        break;
    case N_NEG: case N_NOT: sema_expr(n->a); break;
    case N_AND: case N_OR: sema_expr(n->a); sema_expr(n->b); break;
    case N_BIN: sema_expr(n->a); sema_expr(n->b); break;
    case N_CALL: {
        int fi = find_func(n->name);
        if (fi < 0) compile_error(n->line, "undefined function '%s'", n->name);
        if (G->funcs[fi]->nparams != n->n)
            compile_error(n->line, "function '%s' expects %d argument(s) but %d given",
                          n->name, G->funcs[fi]->nparams, n->n);
        n->func = fi;
        for (i = 0; i < n->n; i++) sema_expr(n->list[i]);
        break;
    }
    default: break;
    }
}

static void sema_block(Node *b, int open) {
    int i;
    if (open) push_scope(b->line);
    for (i = 0; i < b->n; i++) sema_stmt(b->list[i]);
    if (open) pop_scope();
}

static void sema_stmt(Node *n) {
    switch (n->kind) {
    case N_BLOCK: sema_block(n, 1); break;
    case N_DECL:
        if (n->a) sema_expr(n->a);          /* "int x = x;" must not see the new x */
        n->slot = declare(n->name, n->line);
        break;
    case N_ASSIGN:
        sema_expr(n->a);
        n->slot = lookup(n->name);
        if (n->slot < 0) compile_error(n->line, "undeclared variable '%s'", n->name);
        break;
    case N_IF:
        sema_expr(n->a);
        sema_stmt(n->b);
        if (n->c) sema_stmt(n->c);
        break;
    case N_WHILE:
        sema_expr(n->a);
        loop_depth++;
        sema_stmt(n->b);
        loop_depth--;
        if (n->c) sema_stmt(n->c);
        break;
    case N_BREAK: case N_CONTINUE:
        if (loop_depth == 0)
            compile_error(n->line, "'%s' outside a loop", n->kind == N_BREAK ? "break" : "continue");
        break;
    case N_RETURN: case N_PRINT: case N_EXPR: sema_expr(n->a); break;
    default: break;
    }
}

void analyze(Program *prog) {
    int i, j, main_idx = -1;
    G = prog;

    for (i = 0; i < prog->nfuncs; i++) {
        for (j = 0; j < i; j++)
            if (strcmp(prog->funcs[i]->name, prog->funcs[j]->name) == 0)
                compile_error(prog->funcs[i]->line, "function '%s' is already defined", prog->funcs[i]->name);
        if (strcmp(prog->funcs[i]->name, "main") == 0) main_idx = i;
    }
    if (main_idx < 0) compile_error(1, "program must define 'int main()'");
    prog->main_idx = main_idx;
    if (prog->funcs[main_idx]->nparams != 0)
        compile_error(prog->funcs[main_idx]->line, "'main' must not take parameters");

    for (i = 0; i < prog->nfuncs; i++) {
        Node *f = prog->funcs[i];
        nsyms = 0;
        depth = 0;
        bases[0] = 0;
        next_slot = 0;
        loop_depth = 0;
        for (j = 0; j < f->nparams; j++)
            f->params[j]->slot = declare(f->params[j]->name, f->params[j]->line);
        sema_block(f->a, 0); /* body shares the parameter scope */
        f->nlocals = next_slot;
    }
}
