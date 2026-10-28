/* ast.c - pretty printer for the syntax tree (mbc ast file.mc) */
#include "ast.h"

static const char *BINOPS[] = { "+", "-", "*", "/", "<", ">", "<=", ">=", "==", "!=", "%" };

const char *binop_name(int op) { return BINOPS[op]; }

static void pr(const Node *n, FILE *f, int depth) {
    int i;
    if (!n) return;
    fprintf(f, "%*s", depth * 2, "");
    switch (n->kind) {
    case N_NUM: fprintf(f, "Num %d\n", n->value); break;
    case N_VAR: fprintf(f, "Var %s", n->name); if (n->slot >= 0) fprintf(f, "  [slot %d]", n->slot); fputc('\n', f); break;
    case N_NEG: fprintf(f, "Neg\n"); pr(n->a, f, depth + 1); break;
    case N_NOT: fprintf(f, "Not\n"); pr(n->a, f, depth + 1); break;
    case N_AND: fprintf(f, "And\n"); pr(n->a, f, depth + 1); pr(n->b, f, depth + 1); break;
    case N_OR: fprintf(f, "Or\n"); pr(n->a, f, depth + 1); pr(n->b, f, depth + 1); break;
    case N_BIN: fprintf(f, "Bin %s\n", binop_name(n->op)); pr(n->a, f, depth + 1); pr(n->b, f, depth + 1); break;
    case N_CALL:
        fprintf(f, "Call %s\n", n->name);
        for (i = 0; i < n->n; i++) pr(n->list[i], f, depth + 1);
        break;
    case N_DECL: fprintf(f, "Decl %s", n->name); if (n->slot >= 0) fprintf(f, "  [slot %d]", n->slot); fputc('\n', f); pr(n->a, f, depth + 1); break;
    case N_ASSIGN: fprintf(f, "Assign %s", n->name); if (n->slot >= 0) fprintf(f, "  [slot %d]", n->slot); fputc('\n', f); pr(n->a, f, depth + 1); break;
    case N_IF:
        fprintf(f, "If\n");
        pr(n->a, f, depth + 1);
        pr(n->b, f, depth + 1);
        if (n->c) { fprintf(f, "%*sElse\n", depth * 2, ""); pr(n->c, f, depth + 1); }
        break;
    case N_WHILE:
        fprintf(f, "While\n");
        pr(n->a, f, depth + 1);
        pr(n->b, f, depth + 1);
        if (n->c) { fprintf(f, "%*sStep\n", depth * 2, ""); pr(n->c, f, depth + 1); }
        break;
    case N_BREAK: fprintf(f, "Break\n"); break;
    case N_CONTINUE: fprintf(f, "Continue\n"); break;
    case N_RETURN: fprintf(f, "Return\n"); pr(n->a, f, depth + 1); break;
    case N_PRINT: fprintf(f, "Print\n"); pr(n->a, f, depth + 1); break;
    case N_EXPR: fprintf(f, "ExprStmt\n"); pr(n->a, f, depth + 1); break;
    case N_BLOCK:
        fprintf(f, "Block\n");
        for (i = 0; i < n->n; i++) pr(n->list[i], f, depth + 1);
        break;
    case N_FUNC:
        fprintf(f, "Function %s(", n->name);
        for (i = 0; i < n->nparams; i++) fprintf(f, "%s%s", i ? ", " : "", n->params[i]->name);
        fprintf(f, ")");
        if (n->nlocals) fprintf(f, "  [%d local slot(s)]", n->nlocals);
        fputc('\n', f);
        pr(n->a, f, depth + 1);
        break;
    }
}

void ast_print(const Program *prog, FILE *out) {
    int i;
    for (i = 0; i < prog->nfuncs; i++) pr(prog->funcs[i], out, 0);
}
