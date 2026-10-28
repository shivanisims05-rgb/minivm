/* parser.c - recursive-descent parser: one C function per grammar rule.
 *
 * program   ::= function*
 * function  ::= "int" ID "(" params? ")" block
 * params    ::= "int" ID ("," "int" ID)*
 * block     ::= "{" statement* "}"
 * statement ::= "int" ID ("=" expr)? ";"
 *             | ID "=" expr ";"
 *             | ID "(" args? ")" ";"
 *             | "if" "(" expr ")" block ("else" (if-statement | block))?
 *             | "while" "(" expr ")" block
 *             | "for" "(" init? ";" expr? ";" step? ")" block
 *             | "break" ";"  |  "continue" ";"
 *             | "return" expr ";"
 *             | "print" "(" expr ")" ";"
 *             | block
 * init      ::= "int" ID ("=" expr)?  |  ID "=" expr
 * step      ::= ID "=" expr
 * expr      ::= and ("||" and)*
 * and       ::= cmp ("&&" cmp)*
 * cmp       ::= arith (("<" | ">" | "<=" | ">=" | "==" | "!=") arith)?
 * arith     ::= term (("+" | "-") term)*
 * term      ::= unary (("*" | "/" | "%") unary)*
 * unary     ::= ("-" | "!") unary | factor
 * factor    ::= NUMBER | ID | ID "(" args? ")" | "(" expr ")"
 *
 * AST child layout:
 *   N_BIN / N_AND / N_OR: a=left b=right      N_NEG / N_NOT: a=operand       N_CALL: list=args
 *   "for" has no node of its own: it is rewritten to  { init; while (cond) body step=step }
 *   (N_WHILE: a=cond b=body c=step(opt); "continue" jumps to the step, so a for loop still advances)
 *   N_DECL: a=initialiser(opt) N_ASSIGN: a=value
 *   N_IF: a=cond b=then c=else(opt)   N_WHILE: a=cond b=body
 *   N_RETURN / N_PRINT / N_EXPR: a=expression
 *   N_FUNC: params + a=body block
 */
#include "parser.h"

typedef struct { Token *t; int pos; } P;

static Token *peek(P *p) { return &p->t[p->pos]; }
static Token *peek2(P *p) {
    return p->t[p->pos].type == T_EOF ? &p->t[p->pos] : &p->t[p->pos + 1];
}
static Token *advance(P *p) {
    Token *t = &p->t[p->pos];
    if (t->type != T_EOF) p->pos++;
    return t;
}

static Token *expect(P *p, TokType ty) {
    Token *t = peek(p);
    if (t->type != ty) {
        char d[96];
        tok_describe(t, d, sizeof d);
        compile_error(t->line, "expected %s but found %s", tok_name(ty), d);
    }
    return advance(p);
}

static Node *mk(NodeKind k, int line) {
    Node *n = xcalloc(1, sizeof(Node));
    n->kind = k;
    n->line = line;
    n->slot = -1;
    n->func = -1;
    return n;
}

static void push_node(Node ***list, int *n, Node *x) {
    *list = xrealloc(*list, (size_t)(*n + 1) * sizeof(Node *));
    (*list)[(*n)++] = x;
}

static void set_name(Node *n, const char *s) { copy_name(n->name, s); }

static Node *parse_expr(P *p);
static Node *parse_stmt(P *p);
static Node *parse_block(P *p);

/* ---- expressions ---- */

static Node *parse_call(P *p, Token *name) { /* name already consumed, next is '(' */
    Node *n = mk(N_CALL, name->line);
    set_name(n, name->text);
    expect(p, T_LPAREN);
    if (peek(p)->type != T_RPAREN) {
        do {
            push_node(&n->list, &n->n, parse_expr(p));
        } while (peek(p)->type == T_COMMA && advance(p));
    }
    expect(p, T_RPAREN);
    return n;
}

static Node *parse_factor(P *p) {
    Token *t = peek(p);
    char d[96];
    if (t->type == T_NUM) {
        Node *n = mk(N_NUM, t->line);
        advance(p);
        n->value = t->value;
        return n;
    }
    if (t->type == T_ID) {
        advance(p);
        if (peek(p)->type == T_LPAREN) return parse_call(p, t);
        {
            Node *n = mk(N_VAR, t->line);
            set_name(n, t->text);
            return n;
        }
    }
    if (t->type == T_LPAREN) {
        Node *e;
        advance(p);
        e = parse_expr(p);
        expect(p, T_RPAREN);
        return e;
    }
    tok_describe(t, d, sizeof d);
    compile_error(t->line, "expected an expression but found %s", d);
}

static Node *parse_unary(P *p) {
    Token *t = peek(p);
    if (t->type == T_MINUS || t->type == T_NOT) {
        Node *n = mk(t->type == T_MINUS ? N_NEG : N_NOT, t->line);
        advance(p);
        n->a = parse_unary(p);
        return n;
    }
    return parse_factor(p);
}

static Node *make_bin(int op, int line, Node *l, Node *r) {
    Node *n = mk(N_BIN, line);
    n->op = op;
    n->a = l;
    n->b = r;
    return n;
}

static Node *parse_term(P *p) {
    Node *l = parse_unary(p);
    while (peek(p)->type == T_STAR || peek(p)->type == T_SLASH || peek(p)->type == T_PERCENT) {
        Token *t = advance(p);
        Node *r = parse_unary(p);
        l = make_bin(t->type == T_STAR ? B_MUL : t->type == T_SLASH ? B_DIV : B_MOD, t->line, l, r);
    }
    return l;
}

static Node *parse_arith(P *p) {
    Node *l = parse_term(p);
    while (peek(p)->type == T_PLUS || peek(p)->type == T_MINUS) {
        Token *t = advance(p);
        Node *r = parse_term(p);
        l = make_bin(t->type == T_PLUS ? B_ADD : B_SUB, t->line, l, r);
    }
    return l;
}

static Node *parse_cmp(P *p) {
    Node *l = parse_arith(p);
    int op = -1;
    switch (peek(p)->type) {
    case T_LT: op = B_LT; break;
    case T_GT: op = B_GT; break;
    case T_LE: op = B_LE; break;
    case T_GE: op = B_GE; break;
    case T_EQ: op = B_EQ; break;
    case T_NE: op = B_NE; break;
    default: break;
    }
    if (op >= 0) {
        Token *t = advance(p);
        Node *r = parse_arith(p);
        return make_bin(op, t->line, l, r);
    }
    return l;
}

static Node *parse_and(P *p) {
    Node *l = parse_cmp(p);
    while (peek(p)->type == T_AND) {
        Token *t = advance(p);
        Node *n = mk(N_AND, t->line);
        n->a = l;
        n->b = parse_cmp(p);
        l = n;
    }
    return l;
}

static Node *parse_expr(P *p) {
    Node *l = parse_and(p);
    while (peek(p)->type == T_OR) {
        Token *t = advance(p);
        Node *n = mk(N_OR, t->line);
        n->a = l;
        n->b = parse_and(p);
        l = n;
    }
    return l;
}

/* ---- statements ---- */

static Node *parse_block(P *p) {
    Token *lb = expect(p, T_LBRACE);
    Node *b = mk(N_BLOCK, lb->line);
    while (peek(p)->type != T_RBRACE) {
        if (peek(p)->type == T_EOF) compile_error(lb->line, "unclosed '{' (missing '}')");
        push_node(&b->list, &b->n, parse_stmt(p));
    }
    expect(p, T_RBRACE);
    return b;
}

static Node *parse_if(P *p) {
    Token *t = expect(p, T_IF);
    Node *n = mk(N_IF, t->line);
    expect(p, T_LPAREN);
    n->a = parse_expr(p);
    expect(p, T_RPAREN);
    n->b = parse_block(p);
    if (peek(p)->type == T_ELSE) {
        advance(p);
        n->c = (peek(p)->type == T_IF) ? parse_if(p) : parse_block(p);
    }
    return n;
}

/* "int x;" or "int x = expr;"  (the ';' is consumed too) */
static Node *parse_decl(P *p) {
    Token *t = expect(p, T_INT);
    Token *name = expect(p, T_ID);
    Node *n = mk(N_DECL, t->line);
    set_name(n, name->text);
    if (peek(p)->type == T_ASSIGN) { advance(p); n->a = parse_expr(p); }
    expect(p, T_SEMI);
    return n;
}

/* "x = expr" without the ';' (used by the step part of a for loop) */
static Node *parse_assign_noterm(P *p) {
    Token *t = expect(p, T_ID);
    Node *n = mk(N_ASSIGN, t->line);
    set_name(n, t->text);
    expect(p, T_ASSIGN);
    n->a = parse_expr(p);
    return n;
}

/* for (init; cond; step) body  ==>  { init; while (cond) { body; step; } } */
static Node *parse_for(P *p) {
    Token *t = expect(p, T_FOR);
    Node *outer = mk(N_BLOCK, t->line), *loop = mk(N_WHILE, t->line), *inner = mk(N_BLOCK, t->line);
    Node *step = NULL;
    expect(p, T_LPAREN);
    if (peek(p)->type == T_SEMI) advance(p);
    else if (peek(p)->type == T_INT) push_node(&outer->list, &outer->n, parse_decl(p));
    else {
        push_node(&outer->list, &outer->n, parse_assign_noterm(p));
        expect(p, T_SEMI);
    }
    if (peek(p)->type == T_SEMI) { loop->a = mk(N_NUM, t->line); loop->a->value = 1; }
    else loop->a = parse_expr(p);
    expect(p, T_SEMI);
    if (peek(p)->type != T_RPAREN) step = parse_assign_noterm(p);
    expect(p, T_RPAREN);
    push_node(&inner->list, &inner->n, parse_block(p));
    loop->b = inner;
    loop->c = step;
    push_node(&outer->list, &outer->n, loop);
    return outer;
}

static Node *parse_stmt(P *p) {
    Token *t = peek(p);
    Node *n;
    char d[96];
    switch (t->type) {
    case T_INT: return parse_decl(p);
    case T_FOR: return parse_for(p);
    case T_BREAK: case T_CONTINUE:
        advance(p);
        n = mk(t->type == T_BREAK ? N_BREAK : N_CONTINUE, t->line);
        expect(p, T_SEMI);
        return n;
    case T_ID:
        if (peek2(p)->type == T_ASSIGN) {
            n = parse_assign_noterm(p);
            expect(p, T_SEMI);
            return n;
        }
        if (peek2(p)->type == T_LPAREN) {
            advance(p);
            n = mk(N_EXPR, t->line);
            n->a = parse_call(p, t);
            expect(p, T_SEMI);
            return n;
        }
        compile_error(t->line, "expected '=' or '(' after identifier '%s'", t->text);
    case T_IF: return parse_if(p);
    case T_WHILE:
        advance(p);
        n = mk(N_WHILE, t->line);
        expect(p, T_LPAREN);
        n->a = parse_expr(p);
        expect(p, T_RPAREN);
        n->b = parse_block(p);
        return n;
    case T_RETURN:
        advance(p);
        n = mk(N_RETURN, t->line);
        n->a = parse_expr(p);
        expect(p, T_SEMI);
        return n;
    case T_PRINT:
        advance(p);
        n = mk(N_PRINT, t->line);
        expect(p, T_LPAREN);
        n->a = parse_expr(p);
        expect(p, T_RPAREN);
        expect(p, T_SEMI);
        return n;
    case T_LBRACE: return parse_block(p);
    default:
        tok_describe(t, d, sizeof d);
        compile_error(t->line, "unexpected %s at the start of a statement", d);
    }
}

static Node *parse_function(P *p) {
    Token *t = expect(p, T_INT);
    Token *name = expect(p, T_ID);
    Node *f = mk(N_FUNC, t->line);
    set_name(f, name->text);
    expect(p, T_LPAREN);
    if (peek(p)->type != T_RPAREN) {
        do {
            Token *pn;
            Node *d;
            expect(p, T_INT);
            pn = expect(p, T_ID);
            d = mk(N_DECL, pn->line);
            set_name(d, pn->text);
            push_node(&f->params, &f->nparams, d);
        } while (peek(p)->type == T_COMMA && advance(p));
    }
    expect(p, T_RPAREN);
    f->a = parse_block(p);
    return f;
}

Program *parse(TokenList *tl) {
    P p;
    Program *prog = xcalloc(1, sizeof *prog);
    p.t = tl->toks;
    p.pos = 0;
    while (peek(&p)->type != T_EOF) {
        Node *f = parse_function(&p);
        prog->funcs = xrealloc(prog->funcs, (size_t)(prog->nfuncs + 1) * sizeof(Node *));
        prog->funcs[prog->nfuncs++] = f;
    }
    return prog;
}
