/* parser.h - phase 2: tokens -> AST (recursive descent) */
#ifndef PARSER_H
#define PARSER_H

#include "ast.h"
#include "lexer.h"

Program *parse(TokenList *tl);

#endif
