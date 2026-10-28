/* ast.h - abstract syntax tree */
#ifndef AST_H
#define AST_H

#include "common.h"

typedef enum {
    /* expressions */
    N_NUM, N_VAR, N_NEG, N_NOT, N_AND, N_OR, N_BIN, N_CALL,
    /* statements */
    N_DECL, N_ASSIGN, N_IF, N_WHILE, N_BREAK, N_CONTINUE, N_RETURN, N_PRINT, N_EXPR, N_BLOCK,
    /* top level */
    N_FUNC
} NodeKind;

typedef enum { B_ADD, B_SUB, B_MUL, B_DIV, B_LT, B_GT, B_LE, B_GE, B_EQ, B_NE, B_MOD } BinOp;

typedef struct Node Node;
struct Node {
    NodeKind kind;
    int line;
    int value;            /* N_NUM */
    int op;               /* N_BIN: BinOp */
    char name[MAX_NAME];  /* N_VAR, N_ASSIGN, N_DECL, N_CALL, N_FUNC */
    int slot;             /* filled by semantic analysis: local variable slot */
    int func;             /* filled by semantic analysis: callee index (N_CALL) */
    Node *a, *b, *c;      /* children, meaning depends on kind (see parser.c) */
    Node **list; int n;   /* N_BLOCK statements, N_CALL arguments */
    Node **params; int nparams; /* N_FUNC parameters (N_DECL nodes) */
    int nlocals;          /* N_FUNC: total local slots, set by semantic analysis */
};

typedef struct {
    Node **funcs;
    int nfuncs;
    int main_idx;   /* index of "main", set by semantic analysis */
} Program;

const char *binop_name(int op);
void ast_print(const Program *prog, FILE *out);

#endif
