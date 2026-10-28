/* sema.h - phase 3: semantic analysis (scopes, symbol table, call checking) */
#ifndef SEMA_H
#define SEMA_H

#include "ast.h"

/* Checks the program and annotates the AST:
 *   - every variable use/assignment/declaration gets a local slot number
 *   - every call gets the index of the function it calls
 *   - every function gets its total number of local slots */
void analyze(Program *prog);

#endif
