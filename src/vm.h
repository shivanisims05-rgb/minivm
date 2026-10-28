/* vm.h - the virtual machine */
#ifndef VM_H
#define VM_H

#include "bytecode.h"

typedef struct {
    FILE *out;    /* where PRINT writes; NULL discards output (used by bench) */
    int trace;    /* print every executed instruction with the stack after it */
} VmOptions;

typedef struct {
    long instructions;   /* number of instructions executed */
    int exit_value;      /* value returned by main */
    int err_pc;          /* address of the failing instruction */
    char err[160];       /* runtime error message (empty if none) */
} VmResult;

/* Returns 0 on success, 1 on a runtime error (details in r->err). */
int vm_run(const BcProgram *p, const VmOptions *o, VmResult *r);

#endif
