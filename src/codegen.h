/* codegen.h - phase 4: AST -> bytecode */
#ifndef CODEGEN_H
#define CODEGEN_H

#include "ast.h"
#include "bytecode.h"

BcProgram *generate(const Program *prog);

#endif
