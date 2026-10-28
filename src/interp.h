/* interp.h - tree-walking interpreter (the baseline we compare the VM against) */
#ifndef INTERP_H
#define INTERP_H

#include "ast.h"

typedef struct {
    long calls;        /* number of function calls executed */
    int exit_value;    /* value returned by main */
    char err[160];     /* runtime error message (empty if none) */
} InterpResult;

/* Runs an analysed program by walking the AST directly. 0 = ok, 1 = runtime error. */
int interp_run(const Program *prog, FILE *out, InterpResult *r);

#endif
