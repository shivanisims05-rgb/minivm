#!/bin/sh
# Guided demo for the complete project review.
#   make demo                 runs straight through
#   PAUSE=1 sh demo.sh        waits for Enter between steps (good for a live demo)

cd "$(dirname "$0")" || exit 1
[ -x ./mbc ] || make || exit 1

step() {
    printf '\n\033[1m=== %s ===\033[0m\n' "$1"
    if [ -n "$PAUSE" ]; then printf '[press Enter] '; read _unused; fi
}

step "1. Source program (examples/demo.mc)"
cat examples/demo.mc

step "2. Lexer: source -> tokens (first 20)"
./mbc tokens examples/demo.mc | head -20

step "3. Parser + semantic analysis: the syntax tree (slots filled in by the symbol table)"
./mbc ast examples/demo.mc

step "4. Code generator: bytecode (disassembly)"
./mbc dis examples/demo.mc

step "5. Same program with constant folding (-O): 2 + 3 * 4 becomes PUSH 14"
./mbc dis examples/demo.mc -O | tail -8

step "6. Virtual machine: run it"
./mbc run examples/demo.mc --stats

step "7. Watch the VM work: every instruction with the operand stack after it (2 + 3 * 4)"
./mbc run examples/expr.mc --trace | head -16

step "8. Compile to a bytecode file, then run the file on its own"
./mbc compile examples/factorial.mc -o /tmp/factorial.mbc
./mbc exec /tmp/factorial.mbc | tail -4

step "9. Compile-time errors are reported with a line number"
./mbc run examples/errors/undeclared.mc
./mbc run examples/errors/syntax.mc

step "10. Runtime errors are caught by the VM instead of crashing"
./mbc run examples/errors/divzero.mc
./mbc run examples/errors/infinite.mc

step "11. Second back end: the tree-walking interpreter gives the same answer"
./mbc interp examples/primes.mc | tr '\n' ' '; echo

step "12. Benchmark: VM vs tree-walking interpreter"
./mbc bench examples/bench_fib.mc
./mbc bench examples/bench_loop.mc

step "13. New language features: % && || ! and for"
./mbc run examples/modulo.mc | tr '\n' ' '; echo
./mbc run examples/logic.mc | tr '\n' ' '; echo
./mbc run examples/loops.mc | tr '\n' ' '; echo

step "14. Short circuit: the right side of && is skipped (look for JZ and JMP, there is no AND opcode)"
./mbc dis examples/logic.mc | head -30

step "15. Perfect numbers with for, %, && (a larger program)"
./mbc run examples/showcase.mc | tr '\n' ' '; echo

step "16. break, continue and dead-code removal"
./mbc run examples/breaks.mc | tr "\n" " "; echo
./mbc run examples/deadcode.mc -O --stats

step "17. Test suite"
sh tests/run_tests.sh | tail -4
