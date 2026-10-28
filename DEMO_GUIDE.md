# Demo guide for the complete project review (about 10 minutes)

Start it:  `make web`  then open  http://localhost:8000  (full screen, browser zoom 100 to 125%).
The coloured strip at the top is the whole compiler pipeline. The green box under it explains the
current phase in plain words, so you can read it out. Use **Next phase** to move along.

## Script

| Step | Click | Say |
|---|---|---|
| 1. Source | Example `showcase`, stage 1 | "This is MiniC, our own language: int, functions, recursion, if, while, for, and the operators + - * / % && || !. Everything you see is the real output of our C program, nothing is mocked up." |
| 2. Lexer | Next phase | "Characters become tokens. Each token keeps its line number. The bars show how many keywords, names, numbers and operators the program has." |
| 3. Parser | Next phase | "Recursive descent builds this tree. See `*` is below `+`: precedence is in the shape of the tree. A `for` loop is rewritten as a `while` here, so later stages need no new code for it. The symbol table on the right gives every variable a slot number." |
| 4. Optimiser | Example `expr`, Next phase | "Constant folding: `2 + 3 * 4` becomes `PUSH 14` at compile time. Red lines vanish, green lines replace them. The cards show the program shrank from 47 to 23 words and the VM runs 28 instructions instead of 12." |
| 5. Code generator | Next phase | "Post-order walk of the tree gives stack code. Each line has a plain-English meaning. Now the full-project feature: **Save and run the file**. We wrote a `.mbc` file, and the VM runs it without the source. Now **Corrupt an instruction**: the VM refuses it with an error instead of crashing." |
| 6. Virtual machine | Example `recursion`, Next phase, press Next about 15 times (or the Right arrow key) | "Fetch, decode, execute. The yellow line is the instruction, the sentence says what it does, the tower is the operand stack. Each call to `sum_to` pushes a new frame with its own `n`. Frames pile up, then unwind." |
| 8. Verification | Next phase | "The same program ran three ways and all agree. Press **Run test suite**: 103 checks. Press **Run random check**: random programs using every operator, compared across all three implementations, plus random damaged bytecode files fed to the VM." |
| 9. Performance | Next phase, **bench_fib** | "We measured the VM against the tree-walking interpreter. They are close and the winner depends on the program. We do not claim a speed advantage; the VM is built for clarity and safety." |

## Things worth typing live

* **Compile error:** replace the source with `int main() { print(y); return 0; }`, press Compile & Run.
  The stage list shows where it stopped, line 3 turns red in the editor, later stages say "not reached".
* **Run-time error:** `int main() { print(5 % 0); return 0; }`. Go to the Virtual machine phase, step to the end.
* **Short circuit:** pick `logic`. "`0 && (1 / 0)` does not divide by zero, because `&&` skips the right side. It is built from `JZ` and `JMP` only."
* **Dead code:** pick `deadcode`, tick Constant folding, open the Optimiser phase. "Statements after `return`, `if (0)` branches and `while (0)` loops can never run, so the optimiser deletes them. The red lines are gone, and the output is the same."
* **break and continue:** pick `breaks`. "`continue` in a `for` loop jumps to the step, otherwise the loop would never advance. `break` jumps to the first instruction after the loop. Both are just `JMP` with a target patched later."
* **Breakpoint:** pick `recursion`, go to the Virtual machine phase, click the `RET` instruction in the listing (a red marker appears), press **To breakpoint** repeatedly. "The debugger stops each time `sum_to` returns." Click the depth chart to jump to the deepest point.
* **Toggle Optimise (-O)** in the top bar to see every stage change.

## What is new compared with the partial demo

| Area | Partial | Complete |
|---|---|---|
| Language | int, functions, if, while | adds `for`, `break`, `continue`, `%`, `&&`, `||`, `!` |
| Optimiser | none | constant folding and dead-code removal, with a side-by-side diff |
| Second back end | none | tree-walking interpreter and cross-check |
| Bytecode files | none | `.mbc` save, hex dump, run, corruption tests |
| Verification | basic | 103 tests, random-program check |
| VM view | step through | breakpoints, call-depth chart |
| Benchmark | none | live VM against interpreter |
| Opcodes | 16 | 17 (adds `MOD`) |

## Questions sir may ask

* **Why a stack machine?** Simple code generation (post-order walk), compact bytecode, no register allocation. JVM and CPython work this way.
* **Why no `>` or `<=` opcode?** Only `LT` and `EQ` exist; `a > b` is `b a LT`, `a <= b` is `LT` then logical not. Fewer opcodes, smaller VM.
* **How does `&&` short-circuit?** `a JZ Lfalse`, then `b`, then turn it into 0 or 1, `JMP Lend`, `Lfalse: PUSH 0`. If `a` is 0 the jump skips `b`. `||` is the mirror image.
* **How does `for` work?** The parser rewrites `for (i = 0; i < n; i = i + 1) body` into `i = 0; while (i < n) { body; i = i + 1; }`. No new opcodes.
* **How is `%` defined?** Truncating, like C: `-7 % 3` is `-1`. Division or remainder by zero is a run-time error, and the optimiser deliberately does not fold it.
* **How are jumps for `while` generated?** Emit `JZ` with a blank target, generate the body, emit `JMP` back to the test, then patch the `JZ` target.
* **How do function calls work?** `CALL addr` reads the header `[params][locals]`, moves arguments into a new frame, saves the return address. `RET` restores it.
* **What does the VM check at run time?** Stack overflow and underflow, division by zero, bad opcode, bad jump or call target, bad local slot, call depth over 5000.
* **Is the VM fast?** Honest answer: close to the tree-walker, sometimes ahead and sometimes behind. It favours simplicity and safety. Threaded dispatch and superinstructions are the usual next step.
* **How do you know it is correct?** Every example runs four ways (VM, VM with -O, tree-walker, compile to .mbc then exec) against hand-computed answers, plus error cases, plus random programs and random garbage bytecode.

## Terminal backup (if the browser is a problem)

`PAUSE=1 sh demo.sh` walks through the same phases in the terminal, pressing Enter between steps.
