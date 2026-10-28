/* lexer.h - phase 1: characters -> tokens */
#ifndef LEXER_H
#define LEXER_H

#include "common.h"

typedef enum {
    T_EOF, T_NUM, T_ID,
    /* keywords */
    T_INT, T_IF, T_ELSE, T_WHILE, T_FOR, T_BREAK, T_CONTINUE, T_RETURN, T_PRINT,
    /* operators */
    T_PLUS, T_MINUS, T_STAR, T_SLASH, T_PERCENT,
    T_LT, T_GT, T_LE, T_GE, T_EQ, T_NE, T_ASSIGN, T_NOT, T_AND, T_OR,
    /* delimiters */
    T_LPAREN, T_RPAREN, T_LBRACE, T_RBRACE, T_COMMA, T_SEMI
} TokType;

typedef struct {
    TokType type;
    int value;            /* numeric value for T_NUM */
    char text[MAX_NAME];  /* spelling */
    int line;
} Token;

typedef struct {
    Token *toks;
    int count;
} TokenList;

TokenList lex(const char *src);
const char *tok_name(TokType t);                              /* e.g. "';'" */
void tok_describe(const Token *t, char *buf, size_t n);       /* e.g. "identifier 'x'" */
const char *tok_class(TokType t);                             /* KEYWORD / OPERATOR ... */

#endif
