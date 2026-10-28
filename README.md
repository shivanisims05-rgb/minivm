# mbc - a bytecode compiler and virtual machine

Course project, BCSE307P Compiler Design Lab (VIT Vellore).

`mbc` compiles a small C-like language to bytecode for a stack machine and runs
that bytecode on its own virtual machine. It is the implementation behind the
presentation "Virtual Machine + Bytecode Compiler" (complete version for Review 3).

```
source (.mc) -> lexer -> parser -> semantic analysis -> code generator -> bytecode -> VM
                tokens    AST        symbol table        post-order walk   (.mbc)
```

Everything is plain C99 with no libraries. About 2,000 lines in `src/`, comments included.

## Quick start

```sh
make            # builds ./mbc
make test       # 103 checks, should end with "failed: 0"
make demo       # guided demo of every phase (PAUSE=1 sh demo.sh waits for Enter)
make web        # live demo in the browser at http://localhost:8000

./mbc run examples/factorial.mc
```

You need `make` and a C compiler (gcc, clang, or MinGW). On Windows use WSL,
Git Bash with MinGW, or compile by hand:
`gcc -std=c99 -O2 -o mbc src/*.c`

## Live demo in the browser

```sh
make web        # then open http://localhost:8000
```

A single page (`server.py` + `web/index.html`, Python standard library only). A strip of nine
phases runs across the top, with a plain-language explanation under it and your source on the left:

1. Source (edit any program, or pick one of 19 examples; tick Constant folding for `-O`)
2. Lexer (coloured tokens)
3. Parser + Semantic (syntax tree with variable slots)
4. Optimiser (bytecode before and after constant folding and dead-code removal, red = removed, green = added)
5. Code generator (annotated bytecode, plus save as a `.mbc` file, hex dump, run it from the file,
   and three buttons that damage the file to show the VM refuses it)
6. Virtual machine (step through, operand stack, call frames with named variables, click an instruction to set a breakpoint, a call-depth chart you can click to jump in time)
7. Result (program output and instruction count)
8. Verification (VM, VM with -O and the tree-walking interpreter side by side; the real test suite; random-program check)
9. Performance (VM against the interpreter, measured live)

Every result comes from the real `./mbc` binary. It listens on 127.0.0.1 only. Stop it with
Ctrl+C. Use `PORT=9000 make web` for another port. `DEMO_GUIDE.md` has the talking script.

## Commands

| Command | What it does |
|---|---|
| `mbc run file.mc [-O] [--trace] [--stats]` | compile and run on the VM |
| `mbc compile file.mc -o out.mbc [-O]` | write a bytecode file |
| `mbc exec out.mbc [--trace] [--stats]` | run a bytecode file |
| `mbc dis file.mc` or `out.mbc` | disassemble |
| `mbc tokens file.mc` | show the lexer output |
| `mbc ast file.mc` | show the syntax tree, with variable slots filled in |
| `mbc interp file.mc` | run with the tree-walking interpreter instead of the VM |
| `mbc bench file.mc [-n reps]` | time the VM against the tree-walking interpreter |

`-O` turns on constant folding. `--trace` prints every instruction the VM
executes with the operand stack after it. `--stats` prints the instruction
count to stderr.

## The language

```c
int fact(int n) {
    if (n < 2) { return 1; }
    return n * fact(n - 1);
}

int main() {
    int i = 1;
    while (i <= 5) {
        print(fact(i));
        i = i + 1;
    }
    return 0;
}
```

* One type: 32-bit `int` (arithmetic wraps around on overflow).
* Statements: declaration, assignment, `if` / `else` / `else if`, `while`,
  `for (init; test; step)`, `break`, `continue`, `return`, `print(expr);`, blocks, and calls used as statements.
* Operators: `+ - * / %`, unary `-` and `!`, comparisons `< > <= >= == !=` (give 1 or 0),
  and `&&` `||` which short-circuit (the right side is not evaluated when the left decides).
  Any non-zero value counts as true. `%` truncates like C (`-7 % 3` is `-1`).
* `for` has no opcodes of its own: the parser rewrites it as `{ init; while (test) body }` with the step kept on the loop.
  `continue` jumps to the step, so a `for` loop still advances. `break` jumps past the loop. Using them outside a loop is a compile error.
* Functions can call each other and themselves (recursion), in any order.
  Execution starts at `int main()`. Falling off the end of a function returns 0.
* Variables are local to a function and scoped to their block.
* Comments: `// ...` and `/* ... */`.

Grammar (what `src/parser.c` implements, one C function per rule):

```
program   ::= function*
function  ::= "int" ID "(" params? ")" block
statement ::= "int" ID ("=" expr)? ";"  |  ID "=" expr ";"  |  ID "(" args? ")" ";"
            | "if" "(" expr ")" block ("else" (if-statement | block))?
            | "while" "(" expr ")" block  |  "return" expr ";"
            | "for" "(" init? ";" expr? ";" step? ")" block
            | "break" ";"  |  "continue" ";"
            | "print" "(" expr ")" ";"    |  block
expr      ::= and ("||" and)*
and       ::= cmp ("&&" cmp)*
cmp       ::= arith (("<" | ">" | "<=" | ">=" | "==" | "!=") arith)?
arith     ::= term (("+" | "-") term)*
term      ::= unary (("*" | "/" | "%") unary)*
unary     ::= ("-" | "!") unary | factor
factor    ::= NUMBER | ID | ID "(" args? ")" | "(" expr ")"
```

## The instruction set (17 opcodes)

| Opcode | Operand | Effect |
|---|---|---|
| `PUSH n` | constant | push n |
| `POP` | | discard the top value |
| `LOAD i` | slot | push local variable i |
| `STORE i` | slot | pop into local variable i |
| `ADD SUB MUL DIV MOD` | | pop b, pop a, push a op b (`DIV` and `MOD` trap on b = 0) |
| `LT` | | pop b, pop a, push 1 if a < b else 0 |
| `EQ` | | pop b, pop a, push 1 if a == b else 0 |
| `JMP addr` | address | jump |
| `JZ addr` | address | pop; jump if it was 0 |
| `CALL addr` | address | call the function whose header is at addr |
| `RET` | | return; the result stays on the stack |
| `PRINT` | | pop and print |
| `HALT` | | stop |

Code is an array of 32-bit words: one word for the opcode, plus one word for
the operand if it has one. Each function begins with a two-word header
`[params] [locals]` so that `CALL` needs only one operand. The program starts
with a small stub `CALL main ; HALT`.

Only `LT` and `EQ` exist, so the other comparisons are built from them
(see the comment at the top of `src/codegen.c`): `a > b` is `b a LT`,
`a <= b` is `b a LT` then `PUSH 0 EQ` (logical not), and so on. `&&` and `||` use `JZ`
and `JMP` only: `a && b` is `a JZ Lfalse; b; (turn into 0/1); JMP Lend; Lfalse: PUSH 0`. As a result
the operands of `>` and `<=` are evaluated right to left. The tree-walking
interpreter copies this order so both back ends agree.

Example, `x = 2 + 3 * 4; print(x);` (run `mbc run examples/expr.mc --trace`):

```
addr  bytecode   stack after
0000  PUSH 2     [2]
0002  PUSH 3     [2, 3]
0004  PUSH 4     [2, 3, 4]
0006  MUL        [2, 12]
0007  ADD        [14]
0008  STORE 0    []
0010  LOAD 0     [14]
0012  PRINT      []      prints 14
0013  HALT
```

## How the pieces map to the project

| Presentation | File | What it does |
|---|---|---|
| Lexer | `src/lexer.c` | hand-written scanner: characters to tokens with line numbers |
| Parser, AST | `src/parser.c`, `src/ast.h` | recursive descent, builds the syntax tree |
| Semantic check | `src/sema.c` | symbol table with scopes; checks declarations, calls, `main` |
| Optimiser | `src/opt.c` | `-O`: constant folding, plus removal of unreachable code (after `return`/`break`/`continue`, `if (0)`, `while (0)`) |
| Code generator | `src/codegen.c` | post-order walk, jump backpatching, call patching |
| Bytecode, disassembler | `src/bytecode.c` | opcode table, `.mbc` file format, disassembly |
| Virtual machine | `src/vm.c` | fetch-decode-execute loop with runtime checks |
| Baseline for the benchmark | `src/interp.c` | tree-walking interpreter |
| Driver | `src/main.c` | the command line |

Runtime checks in the VM: operand stack overflow and underflow, division by
zero, invalid opcode, missing operand, bad jump or call target, bad local
slot, and a call depth limit of 5000 (so runaway recursion gives an error
instead of a crash). The VM was also run against random garbage bytecode
files and never crashed.

## Testing

`make test` runs every program in `examples/` four ways (VM, VM with `-O`,
tree-walking interpreter, compile to `.mbc` then exec) and compares each with
a hand-written `.out` file. It then checks that each program in
`examples/errors/` fails with the expected message.

The expected values were worked out by hand: `fact(5) = 120`, `fib(10) = 55`,
`gcd(48, 18) = 6`, `1 + 2 + ... + 100 = 5050`, `fib(27) = 196418`, perfect numbers 6, 28, 496.

The web demo's Verification stage also has a random-program check (random programs using every
operator, compared across the VM, the VM with `-O` and the interpreter).

## Benchmark: what to expect

`mbc bench examples/bench_fib.mc`, `bench_loop.mc` and `bench_perfect.mc` time the VM against
the tree-walking interpreter. The two are close, and which is ahead depends on the program and
the machine: in our runs the VM was somewhat faster on recursive `fib` and slower on tight
loops. That is a real result, not a bug. This VM favours simplicity and safety (a check on every
instruction, plain `switch` dispatch). Threaded dispatch, superinstructions and keeping the program counter in
a register are the usual ways to make a VM faster and would be natural extensions. Your numbers
will differ with the CPU and compiler. Tiny programs mostly measure start-up cost.

## Extending it

* New opcode: add it to `Opcode` and the name table in `src/bytecode.h/.c`
  (and the operand mask in `op_has_operand` if it takes an operand), a `case`
  in `src/vm.c`, and generation in `src/codegen.c`.
* New operator: token in `lexer.c`, a `BinOp` in `ast.h`, precedence level in
  `parser.c`, then `codegen.c`, `interp.c` and `opt.c`.
* Ideas: a constant pool, strings, arrays,
  a register-based VM variant to compare against this one.

## Folder layout

```
Makefile  demo.sh  README.md  DEMO_GUIDE.md  server.py
web/       index.html for the live demo
src/       the compiler, VM and interpreter (C)
examples/  demo programs (.mc) with expected output (.out)
examples/errors/   programs that must be rejected, with the expected message (.err)
tests/run_tests.sh the test suite
```
