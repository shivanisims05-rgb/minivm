/* vm.c - stack-based virtual machine: a fetch-decode-execute loop.
 *
 * State:   pc  program counter (index into the code array)
 *          st  operand stack, sp = number of values on it
 *          loc local-variable memory; each call owns a window [fp, lp)
 *          fr  call stack: return address and caller's frame pointer
 *
 * Runtime checks (so a bad program stops with a message instead of crashing):
 *   operand stack overflow/underflow, division by zero, invalid opcode,
 *   missing operand, bad jump/call target, bad local slot, call depth limit.
 */
#include "vm.h"

#define STACK_MAX  65536
#define LOCALS_MAX 262144
#define CALL_MAX   5000

typedef struct { int ret_pc; int prev_fp; } Frame;

static void fmt_stack(char *buf, size_t n, const int *st, int sp) {
    size_t k = 0;
    int i, start = sp > 8 ? sp - 8 : 0;
    k += (size_t)snprintf(buf + k, n - k, "[%s", start > 0 ? "... " : "");
    for (i = start; i < sp && k < n; i++)
        k += (size_t)snprintf(buf + k, n - k, "%s%d", i > start ? ", " : "", st[i]);
    if (k < n) snprintf(buf + k, n - k, "]");
}

#define FAIL(...) do { snprintf(r->err, sizeof r->err, __VA_ARGS__); r->err_pc = ipc; rc = 1; goto done; } while (0)

int vm_run(const BcProgram *p, const VmOptions *o, VmResult *r) {
    const int *code = p->code;
    const int len = p->len;
    int *st = xcalloc(STACK_MAX, sizeof(int));
    int *loc = xcalloc(LOCALS_MAX, sizeof(int));
    Frame *fr = xcalloc(CALL_MAX, sizeof(Frame));
    int sp = 0, fp = 0, lp = 0, depth = 0, pc = 0, ipc = 0, halted = 0, rc = 0;
    long count = 0;

    memset(r, 0, sizeof *r);
    if (o->trace && o->out) fprintf(o->out, "%-5s %-6s %-6s %s\n", "pc", "op", "arg", "stack after");

    while (!halted) {
        int op, operand = 0, pending = 0, pend_val = 0;

        /* ---- fetch ---- */
        ipc = pc;
        if (pc < 0 || pc >= len) FAIL("program counter out of range");
        op = code[pc++];
        if (op < 0 || op >= OP_COUNT) FAIL("invalid opcode %d", op);
        if (op_has_operand(op)) {
            if (pc >= len) FAIL("instruction is missing its operand");
            operand = code[pc++];
        }
        count++;

        /* ---- decode + execute ---- */
        switch (op) {
        case OP_PUSH:
            if (sp >= STACK_MAX) FAIL("operand stack overflow");
            st[sp++] = operand;
            break;
        case OP_POP:
            if (sp < 1) FAIL("operand stack underflow");
            sp--;
            break;
        case OP_LOAD:
            if (operand < 0 || fp + operand >= lp) FAIL("invalid local slot %d", operand);
            if (sp >= STACK_MAX) FAIL("operand stack overflow");
            st[sp++] = loc[fp + operand];
            break;
        case OP_STORE:
            if (sp < 1) FAIL("operand stack underflow");
            if (operand < 0 || fp + operand >= lp) FAIL("invalid local slot %d", operand);
            loc[fp + operand] = st[--sp];
            break;
        case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_MOD: case OP_LT: case OP_EQ: {
            int b, a, res = 0;
            if (sp < 2) FAIL("operand stack underflow");
            b = st[--sp];
            a = st[--sp];
            switch (op) {
            case OP_ADD: res = w_add(a, b); break;
            case OP_SUB: res = w_sub(a, b); break;
            case OP_MUL: res = w_mul(a, b); break;
            case OP_DIV: if (w_div(a, b, &res)) FAIL("division by zero"); break;
            case OP_MOD: if (w_mod(a, b, &res)) FAIL("division by zero (in %%)"); break;
            case OP_LT:  res = a < b; break;
            default:     res = a == b; break;
            }
            st[sp++] = res;
            break;
        }
        case OP_JMP:
            if (operand < 0 || operand >= len) FAIL("invalid jump target %d", operand);
            pc = operand;
            break;
        case OP_JZ:
            if (sp < 1) FAIL("operand stack underflow");
            if (operand < 0 || operand >= len) FAIL("invalid jump target %d", operand);
            if (st[--sp] == 0) pc = operand;
            break;
        case OP_CALL: {
            int i, np, nl;
            if (operand < 0 || operand + 1 >= len) FAIL("invalid call target %d", operand);
            np = code[operand];
            nl = code[operand + 1];
            if (np < 0 || nl < np) FAIL("corrupt function header at %d", operand);
            if (depth >= CALL_MAX) FAIL("call stack overflow (recursion too deep)");
            if (lp + nl > LOCALS_MAX) FAIL("local variable memory exhausted (recursion too deep)");
            if (sp < np) FAIL("operand stack underflow");
            fr[depth].ret_pc = pc;
            fr[depth].prev_fp = fp;
            depth++;
            fp = lp;
            lp += nl;
            memset(&loc[fp], 0, (size_t)nl * sizeof(int));
            for (i = np - 1; i >= 0; i--) loc[fp + i] = st[--sp]; /* last argument was pushed last */
            pc = operand + 2;
            break;
        }
        case OP_RET:
            if (depth == 0) FAIL("return with an empty call stack");
            depth--;
            lp = fp;
            fp = fr[depth].prev_fp;
            pc = fr[depth].ret_pc;
            break;
        case OP_PRINT: {
            int v;
            if (sp < 1) FAIL("operand stack underflow");
            v = st[--sp];
            if (o->trace) { pending = 1; pend_val = v; }
            else if (o->out) fprintf(o->out, "%d\n", v);
            break;
        }
        case OP_HALT:
            halted = 1;
            break;
        }

        /* ---- optional trace ---- */
        if (o->trace && o->out) {
            char sbuf[256];
            fmt_stack(sbuf, sizeof sbuf, st, sp);
            if (op_has_operand(op)) fprintf(o->out, "%04d  %-6s %-6d %s\n", ipc, op_name(op), operand, sbuf);
            else fprintf(o->out, "%04d  %-6s %-6s %s\n", ipc, op_name(op), "", sbuf);
            if (pending) fprintf(o->out, "      output: %d\n", pend_val);
        }
    }

done:
    r->instructions = count;
    r->exit_value = sp > 0 ? st[sp - 1] : 0;
    free(st);
    free(loc);
    free(fr);
    return rc;
}
