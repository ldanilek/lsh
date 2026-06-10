#ifndef PARSER_H
#define PARSER_H

#include <lsh/lexer.h>

AstNode* parse(const char* input);
char* parse_error(void);

#endif
