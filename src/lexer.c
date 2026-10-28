/* lexer.c - hand-written scanner */
#include "lexer.h"
#include <ctype.h>

static const struct { const char *s; TokType t; } KEYWORDS[] = {
    {"int", T_INT}, {"if", T_IF}, {"else", T_ELSE},
    {"while", T_WHILE}, {"for", T_FOR}, {"break", T_BREAK}, {"continue", T_CONTINUE}, {"return", T_RETURN}, {"print", T_PRINT},
    {NULL, T_EOF}
};

const char *tok_name(TokType t) {
    switch (t) {
    case T_EOF: return "end of file";
    case T_NUM: return "number";
    case T_ID: return "identifier";
    case T_INT: return "'int'";
    case T_IF: return "'if'";
    case T_ELSE: return "'else'";
    case T_WHILE: return "'while'";
    case T_FOR: return "'for'";
    case T_BREAK: return "'break'";
    case T_CONTINUE: return "'continue'";
    case T_RETURN: return "'return'";
    case T_PRINT: return "'print'";
    case T_PLUS: return "'+'";
    case T_MINUS: return "'-'";
    case T_STAR: return "'*'";
    case T_SLASH: return "'/'";
    case T_PERCENT: return "'%'";
    case T_LT: return "'<'";
    case T_GT: return "'>'";
    case T_LE: return "'<='";
    case T_GE: return "'>='";
    case T_EQ: return "'=='";
    case T_NE: return "'!='";
    case T_ASSIGN: return "'='";
    case T_NOT: return "'!'";
    case T_AND: return "'&&'";
    case T_OR: return "'||'";
    case T_LPAREN: return "'('";
    case T_RPAREN: return "')'";
    case T_LBRACE: return "'{'";
    case T_RBRACE: return "'}'";
    case T_COMMA: return "','";
    case T_SEMI: return "';'";
    }
    return "?";
}

const char *tok_class(TokType t) {
    switch (t) {
    case T_EOF: return "EOF";
    case T_NUM: return "NUMBER";
    case T_ID: return "IDENT";
    case T_INT: case T_IF: case T_ELSE: case T_WHILE: case T_FOR: case T_RETURN: case T_PRINT:
    case T_BREAK: case T_CONTINUE:
        return "KEYWORD";
    case T_PLUS: case T_MINUS: case T_STAR: case T_SLASH: case T_PERCENT:
    case T_LT: case T_GT: case T_LE: case T_GE: case T_EQ: case T_NE: case T_ASSIGN:
    case T_NOT: case T_AND: case T_OR:
        return "OPERATOR";
    default: return "DELIM";
    }
}

void tok_describe(const Token *t, char *buf, size_t n) {
    if (t->type == T_ID) snprintf(buf, n, "identifier '%s'", t->text);
    else if (t->type == T_NUM) snprintf(buf, n, "number %s", t->text);
    else snprintf(buf, n, "%s", tok_name(t->type));
}

static void push(TokenList *tl, int *cap, TokType t, int line, const char *text, int value) {
    Token *k;
    if (tl->count == *cap) {
        *cap *= 2;
        tl->toks = xrealloc(tl->toks, (size_t)*cap * sizeof(Token));
    }
    k = &tl->toks[tl->count++];
    memset(k, 0, sizeof *k);
    k->type = t;
    k->line = line;
    k->value = value;
    copy_name(k->text, text);
}

TokenList lex(const char *src) {
    TokenList tl;
    int cap = 128, line = 1;
    const char *p = src;

    tl.count = 0;
    tl.toks = xcalloc((size_t)cap, sizeof(Token));

    for (;;) {
        char c = *p;
        if (c == '\0') break;
        if (c == '\n') { line++; p++; continue; }
        if (isspace((unsigned char)c)) { p++; continue; }

        /* comments */
        if (c == '/' && p[1] == '/') {
            while (*p && *p != '\n') p++;
            continue;
        }
        if (c == '/' && p[1] == '*') {
            int start = line;
            p += 2;
            while (*p && !(p[0] == '*' && p[1] == '/')) {
                if (*p == '\n') line++;
                p++;
            }
            if (!*p) compile_error(start, "unterminated /* comment");
            p += 2;
            continue;
        }

        /* numbers */
        if (isdigit((unsigned char)c)) {
            char buf[MAX_NAME];
            int n = 0;
            long long v = 0;
            while (isdigit((unsigned char)*p)) {
                if (n < MAX_NAME - 1) buf[n++] = *p;
                v = v * 10 + (*p - '0');
                if (v > 2147483647LL) compile_error(line, "integer literal is too large");
                p++;
            }
            buf[n] = '\0';
            if (isalpha((unsigned char)*p) || *p == '_')
                compile_error(line, "invalid number '%s%c...'", buf, *p);
            push(&tl, &cap, T_NUM, line, buf, (int)v);
            continue;
        }

        /* identifiers and keywords */
        if (isalpha((unsigned char)c) || c == '_') {
            char buf[MAX_NAME];
            int n = 0, k;
            TokType type = T_ID;
            while (isalnum((unsigned char)*p) || *p == '_') {
                if (n >= MAX_NAME - 1) compile_error(line, "identifier is too long");
                buf[n++] = *p++;
            }
            buf[n] = '\0';
            for (k = 0; KEYWORDS[k].s; k++)
                if (strcmp(KEYWORDS[k].s, buf) == 0) { type = KEYWORDS[k].t; break; }
            push(&tl, &cap, type, line, buf, 0);
            continue;
        }

        /* operators and delimiters */
        {
            TokType t;
            char text[3] = { c, '\0', '\0' };
            int two = 0;
            switch (c) {
            case '+': t = T_PLUS; break;
            case '-': t = T_MINUS; break;
            case '*': t = T_STAR; break;
            case '/': t = T_SLASH; break;
            case '%': t = T_PERCENT; break;
            case '(': t = T_LPAREN; break;
            case ')': t = T_RPAREN; break;
            case '{': t = T_LBRACE; break;
            case '}': t = T_RBRACE; break;
            case ',': t = T_COMMA; break;
            case ';': t = T_SEMI; break;
            case '<': if (p[1] == '=') { t = T_LE; two = 1; } else t = T_LT; break;
            case '>': if (p[1] == '=') { t = T_GE; two = 1; } else t = T_GT; break;
            case '=': if (p[1] == '=') { t = T_EQ; two = 1; } else t = T_ASSIGN; break;
            case '!': if (p[1] == '=') { t = T_NE; two = 1; } else t = T_NOT; break;
            case '&':
                if (p[1] != '&') compile_error(line, "unexpected character '&' (did you mean '&&'?)");
                t = T_AND; two = 1; break;
            case '|':
                if (p[1] != '|') compile_error(line, "unexpected character '|' (did you mean '||'?)");
                t = T_OR; two = 1; break;
            default:
                compile_error(line, "unexpected character '%c'", c);
            }
            if (two) text[1] = p[1];
            push(&tl, &cap, t, line, text, 0);
            p += two ? 2 : 1;
        }
    }
    push(&tl, &cap, T_EOF, line, "", 0);
    return tl;
}
