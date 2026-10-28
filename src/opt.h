/* opt.h - optional optimisation pass (enabled with -O) */
#ifndef OPT_H
#define OPT_H

#include "ast.h"

/* Constant folding: replaces e.g. 2 + 3 * 4 with 14 in the AST.
 * Returns the number of expressions that were folded. */
int fold_constants(Program *prog);

/* Dead-code removal (done in the same pass): statements after return, break
 * or continue, if-statements with a constant condition, and while loops whose
 * condition is constant 0. Number removed by the last fold_constants call. */
int dead_removed(void);

#endif
