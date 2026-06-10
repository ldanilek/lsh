#ifndef LEXER_H
#define LEXER_H

#include <lsh/ast.h>

typedef enum {
    TOK_WORD,
    TOK_PIPE,
    TOK_AMP,
    TOK_SEMI,
    TOK_ANDAND,
    TOK_OROR,
    TOK_LT,
    TOK_GT,
    TOK_GTGT,
    TOK_2GT,
    TOK_2GTGT,
    TOK_ANDGT,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_EOF,
    TOK_ERROR
} TokenType;

typedef struct Token {
    TokenType type;
    char *value;
} Token;

typedef struct Lexer {
    const char *input;
    size_t pos;
    Token current;
} Lexer;

void lexer_init(Lexer *lx, const char *input);
void lexer_next(Lexer *lx);
void token_free(Token *tok);

#endif
