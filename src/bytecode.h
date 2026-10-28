/* bytecode.h - the instruction set, the program image and the disassembler.
 *
 * Code is an array of 32-bit words. An instruction is one opcode word,
 * followed by one operand word if the opcode takes an operand.
 *
 * Every function starts with a 2-word header:  [nparams] [nlocals]
 * and its first instruction follows right after. CALL <addr> points at the
 * header, so CALL only needs a single operand.
 *
 * Program layout:
 *   0: CALL <main>      <- entry stub
 *   2: HALT
 *   3: header + code of the first function, then the next one, ...
 */
#ifndef BYTECODE_H
#define BYTECODE_H

#include "common.h"

typedef enum {
    OP_PUSH,   /* PUSH n    push the constant n                         */
    OP_POP,    /* POP       discard the top of the stack                */
    OP_LOAD,   /* LOAD i    push local variable slot i                  */
    OP_STORE,  /* STORE i   pop into local variable slot i              */
    OP_ADD,    /* ADD       pop b, pop a, push a + b                    */
    OP_SUB,    /* SUB       pop b, pop a, push a - b                    */
    OP_MUL,    /* MUL       pop b, pop a, push a * b                    */
    OP_DIV,    /* DIV       pop b, pop a, push a / b (traps on b == 0)  */
    OP_LT,     /* LT        pop b, pop a, push 1 if a < b else 0        */
    OP_EQ,     /* EQ        pop b, pop a, push 1 if a == b else 0       */
    OP_JMP,    /* JMP addr  jump to addr                                */
    OP_JZ,     /* JZ addr   pop; jump to addr if it was 0               */
    OP_CALL,   /* CALL addr call the function whose header is at addr   */
    OP_RET,    /* RET       return; the result stays on the stack       */
    OP_PRINT,  /* PRINT     pop and print as a decimal line             */
    OP_HALT,   /* HALT      stop the VM                                 */
    OP_MOD,    /* MOD       pop b, pop a, push a % b (traps on b == 0)  */
    OP_COUNT
} Opcode;

typedef struct {
    char name[MAX_NAME];
    int addr;               /* address of the function header */
} FuncInfo;

typedef struct {
    int *code;
    int len, cap;
    FuncInfo *funcs;
    int nfuncs;
} BcProgram;

const char *op_name(int op);
/* opcodes that are followed by an operand word (bit mask, so it inlines) */
static inline int op_has_operand(int op) {
    return op >= 0 && op < OP_COUNT && ((0x1C0Du >> op) & 1u);
}

/* Bytecode files (.mbc): magic "MBC1", code words, function table. */
int bc_save(const BcProgram *p, const char *path);                      /* 1 = ok */
BcProgram *bc_load(const char *path, char *err, size_t errlen);         /* NULL on error */

void disassemble(const BcProgram *p, FILE *out);

#endif
