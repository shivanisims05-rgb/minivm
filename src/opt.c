/* opt.c - constant folding and dead-code removal on the AST.
 * Division by zero is deliberately NOT folded, so the runtime error still
 * happens when (and only when) the program actually executes the division. */
#include "opt.h"

static int folds, dead;

int dead_removed(void) { return dead; }

/* a statement after which control never continues to the next statement */
static int is_jump(const Node *n) {
    int i;
    if (n->kind == N_RETURN || n->kind == N_BREAK || n->kind == N_CONTINUE) return 1;
    if (n->kind == N_BLOCK)
        for (i = 0; i < n->n; i++)
            if (is_jump(n->list[i])) return 1;
    return 0;
}

static Node *empty_block(int line) {
    Node *b = xcalloc(1, sizeof(Node));
    b->kind = N_BLOCK;
    b->line = line;
    b->slot = -1;
    b->func = -1;
    return b;
}

/* dead-code removal for the node whose children have just been folded */
static void remove_dead(Node *n) {
    int i;
    if (n->kind == N_BLOCK) {
        for (i = 0; i < n->n; i++)
            if (is_jump(n->list[i]) && i + 1 < n->n) {
                dead += n->n - (i + 1);
                n->n = i + 1;          /* everything after is unreachable */
                break;
            }
    } else if (n->kind == N_IF && n->a->kind == N_NUM) {
        Node *keep = n->a->value ? n->b : n->c;
        if (!keep) keep = empty_block(n->line);
        *n = *keep;                    /* the if-statement becomes the branch that can run */
        dead++;
    } else if (n->kind == N_WHILE && n->a->kind == N_NUM && n->a->value == 0) {
        *n = *empty_block(n->line);    /* the body can never run */
        dead++;
    }
}

static void fold(Node *n) {
    int i;
    if (!n) return;
    fold(n->a);
    fold(n->b);
    fold(n->c);
    for (i = 0; i < n->n; i++) fold(n->list[i]);
    remove_dead(n);

    if (n->kind == N_NOT && n->a->kind == N_NUM) {
        n->value = n->a->value == 0;
        n->kind = N_NUM;
        n->a = NULL;
        folds++;
    } else if (n->kind == N_AND || n->kind == N_OR) {
        /* only fold when the answer cannot depend on side effects of the right side */
        int lc = n->a->kind == N_NUM, rc = n->b->kind == N_NUM;
        int l = lc ? n->a->value : 0, r = rc ? n->b->value : 0, res;
        if (lc && rc) res = n->kind == N_AND ? (l && r) : (l || r);
        else if (lc && n->kind == N_AND && l == 0) res = 0;     /* 0 && x : x is never evaluated */
        else if (lc && n->kind == N_OR && l != 0) res = 1;      /* 1 || x : x is never evaluated */
        else return;
        n->value = res;
        n->kind = N_NUM;
        n->a = n->b = NULL;
        folds++;
    } else if (n->kind == N_NEG && n->a->kind == N_NUM) {
        n->value = w_sub(0, n->a->value);
        n->kind = N_NUM;
        n->a = NULL;
        folds++;
    } else if (n->kind == N_BIN && n->a->kind == N_NUM && n->b->kind == N_NUM) {
        int a = n->a->value, b = n->b->value, r = 0;
        switch (n->op) {
        case B_ADD: r = w_add(a, b); break;
        case B_SUB: r = w_sub(a, b); break;
        case B_MUL: r = w_mul(a, b); break;
        case B_DIV: if (w_div(a, b, &r)) return; break;
        case B_MOD: if (w_mod(a, b, &r)) return; break;
        case B_LT: r = a < b; break;
        case B_GT: r = a > b; break;
        case B_LE: r = a <= b; break;
        case B_GE: r = a >= b; break;
        case B_EQ: r = a == b; break;
        case B_NE: r = a != b; break;
        }
        n->value = r;
        n->kind = N_NUM;
        n->a = n->b = NULL;
        folds++;
    }
}

int fold_constants(Program *prog) {
    int i;
    folds = 0;
    dead = 0;
    for (i = 0; i < prog->nfuncs; i++) fold(prog->funcs[i]->a);
    return folds;
}
