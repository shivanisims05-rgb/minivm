/* main.c - command line driver: wires the phases together.
 *
 *   source -> lex -> parse -> analyze -> (fold) -> generate -> vm_run
 */
#include <time.h>
#include "common.h"
#include "lexer.h"
#include "parser.h"
#include "sema.h"
#include "opt.h"
#include "codegen.h"
#include "bytecode.h"
#include "vm.h"
#include "interp.h"

static int g_folds = 0, g_dead = 0;

static void usage(void) {
    fputs(
"mbc - mini bytecode compiler and virtual machine\n"
"\n"
"usage:\n"
"  mbc run     <file.mc> [-O] [--trace] [--stats]   compile and run on the VM\n"
"  mbc compile <file.mc> -o <out.mbc> [-O]          compile to a bytecode file\n"
"  mbc exec    <file.mbc> [--trace] [--stats]       run a bytecode file on the VM\n"
"  mbc dis     <file.mc|file.mbc> [-O]              show the bytecode (disassembler)\n"
"  mbc tokens  <file.mc>                            show the lexer output\n"
"  mbc ast     <file.mc> [-O]                       show the syntax tree\n"
"  mbc interp  <file.mc>                            run with the tree-walking interpreter\n"
"  mbc bench   <file.mc> [-O] [-n reps]             VM vs tree-walking interpreter timing\n"
"\n"
"options:\n"
"  -O        constant folding (e.g. 2 + 3 * 4 becomes 14 at compile time)\n"
"  --trace   print every instruction executed with the stack after it\n"
"  --stats   print instruction count and main's return value to stderr\n",
    stderr);
}

static int has_ext(const char *path, const char *ext) {
    size_t a = strlen(path), b = strlen(ext);
    return a >= b && strcmp(path + a - b, ext) == 0;
}

/* front end: source file -> analysed (and optionally folded) AST */
static Program *front_end(const char *path, int opt) {
    char *src;
    TokenList tl;
    Program *prog;
    g_file = path;
    src = read_file(path);
    if (!src) { fprintf(stderr, "mbc: cannot read '%s'\n", path); exit(1); }
    tl = lex(src);
    prog = parse(&tl);
    analyze(prog);
    if (opt) { g_folds = fold_constants(prog); g_dead = dead_removed(); }
    free(src);
    return prog;
}

/* a .mbc file is loaded; anything else is compiled from source */
static BcProgram *get_bytecode(const char *path, int opt) {
    if (has_ext(path, ".mbc")) {
        char err[200];
        BcProgram *p = bc_load(path, err, sizeof err);
        if (!p) { fprintf(stderr, "mbc: %s\n", err); exit(1); }
        return p;
    }
    return generate(front_end(path, opt));
}

static int run_vm(const BcProgram *p, int trace, int stats) {
    VmOptions vo;
    VmResult r;
    int rc;
    vo.out = stdout;
    vo.trace = trace;
    rc = vm_run(p, &vo, &r);
    fflush(stdout);
    if (rc) {
        fprintf(stderr, "runtime error at pc=%d: %s\n", r.err_pc, r.err);
        return 2;
    }
    if (stats) {
        fprintf(stderr, "[stats] instructions executed: %ld, bytecode size: %d words, main returned %d",
                r.instructions, p->len, r.exit_value);
        if (g_folds) fprintf(stderr, ", constant folds: %d", g_folds);
        if (g_dead) fprintf(stderr, ", dead statements removed: %d", g_dead);
        fputc('\n', stderr);
    }
    return 0;
}

static double seconds(clock_t a, clock_t b) { return (double)(b - a) / CLOCKS_PER_SEC; }

static int cmd_bench(const char *path, int opt, int reps) {
    Program *prog = front_end(path, opt);
    BcProgram *bc = generate(prog);
    VmOptions vo;
    VmResult vr;
    InterpResult ir;
    clock_t t0, t1;
    double vm_total = 0, tw_total = 0;
    int i;

    vo.out = NULL;
    vo.trace = 0;
    for (i = 0; i < reps; i++) {
        t0 = clock();
        if (vm_run(bc, &vo, &vr)) { fprintf(stderr, "runtime error at pc=%d: %s\n", vr.err_pc, vr.err); return 2; }
        t1 = clock();
        vm_total += seconds(t0, t1);
        t0 = clock();
        if (interp_run(prog, NULL, &ir)) { fprintf(stderr, "runtime error: %s\n", ir.err); return 2; }
        t1 = clock();
        tw_total += seconds(t0, t1);
    }
    printf("benchmark: %s   (%d runs each, program output discarded%s)\n", path, reps, opt ? ", -O" : "");
    printf("  bytecode VM         : %8.4f s per run   (%ld instructions)\n", vm_total / reps, vr.instructions);
    printf("  tree-walking interp : %8.4f s per run   (%ld function calls)\n", tw_total / reps, ir.calls);
    if (vm_total > 0 && tw_total > 0) {
        if (tw_total >= vm_total) printf("  VM is %.2fx faster than the tree-walking interpreter\n", tw_total / vm_total);
        else printf("  VM is %.2fx slower than the tree-walking interpreter\n", vm_total / tw_total);
    }
    return 0;
}

int main(int argc, char **argv) {
    const char *cmd, *file = NULL, *outfile = NULL;
    int i, opt = 0, trace = 0, stats = 0, reps = 5;

    if (argc < 2 || strcmp(argv[1], "help") == 0 || strcmp(argv[1], "--help") == 0) { usage(); return argc < 2 ? 1 : 0; }
    cmd = argv[1];
    for (i = 2; i < argc; i++) {
        if (argv[i][0] != '-' && !file) file = argv[i];
        else if (strcmp(argv[i], "-O") == 0) opt = 1;
        else if (strcmp(argv[i], "--trace") == 0) trace = 1;
        else if (strcmp(argv[i], "--stats") == 0) stats = 1;
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) outfile = argv[++i];
        else if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) { reps = atoi(argv[++i]); if (reps < 1) reps = 1; }
        else { fprintf(stderr, "mbc: unknown option '%s'\n", argv[i]); usage(); return 1; }
    }
    if (!file) { fprintf(stderr, "mbc: missing input file\n"); usage(); return 1; }

    if (strcmp(cmd, "run") == 0 || strcmp(cmd, "exec") == 0) {
        return run_vm(get_bytecode(file, opt), trace, stats);
    } else if (strcmp(cmd, "compile") == 0) {
        BcProgram *p;
        if (!outfile) { fprintf(stderr, "mbc: compile needs -o <out.mbc>\n"); return 1; }
        p = get_bytecode(file, opt);
        if (!bc_save(p, outfile)) { fprintf(stderr, "mbc: cannot write '%s'\n", outfile); return 1; }
        printf("compiled %s -> %s (%d words, %d function(s))\n", file, outfile, p->len, p->nfuncs);
        return 0;
    } else if (strcmp(cmd, "dis") == 0) {
        disassemble(get_bytecode(file, opt), stdout);
        return 0;
    } else if (strcmp(cmd, "tokens") == 0) {
        char *src = read_file(file);
        TokenList tl;
        int k;
        g_file = file;
        if (!src) { fprintf(stderr, "mbc: cannot read '%s'\n", file); return 1; }
        tl = lex(src);
        printf("%-5s %-9s %s\n", "line", "class", "text");
        for (k = 0; k < tl.count; k++)
            printf("%-5d %-9s %s\n", tl.toks[k].line, tok_class(tl.toks[k].type), tl.toks[k].text);
        return 0;
    } else if (strcmp(cmd, "ast") == 0) {
        ast_print(front_end(file, opt), stdout);
        return 0;
    } else if (strcmp(cmd, "interp") == 0) {
        Program *prog = front_end(file, opt);
        InterpResult r;
        int rc = interp_run(prog, stdout, &r);
        fflush(stdout);
        if (rc) { fprintf(stderr, "runtime error: %s\n", r.err); return 2; }
        if (stats) fprintf(stderr, "[stats] function calls: %ld, main returned %d\n", r.calls, r.exit_value);
        return 0;
    } else if (strcmp(cmd, "bench") == 0) {
        return cmd_bench(file, opt, reps);
    }
    fprintf(stderr, "mbc: unknown command '%s'\n", cmd);
    usage();
    return 1;
}
