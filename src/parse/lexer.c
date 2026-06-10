#include <lsh/lexer.h>

static char peek(const Lexer *lx) {
    return lx->input[lx->pos];
}

static char advance(Lexer *lx) {
    return lx->input[lx->pos++];
}

static void skip_ws(Lexer *lx) {
    while (peek(lx) == ' ' || peek(lx) == '\t' || peek(lx) == '\n')
        advance(lx);
}

static char *read_quoted(Lexer *lx, char quote) {
    size_t cap = 64, len = 0;
    char *buf = malloc(cap);
    if (!buf) return NULL;

    advance(lx); /* skip opening quote */
    while (peek(lx) && peek(lx) != quote) {
        if (peek(lx) == '\\' && quote == '"') {
            advance(lx);
            if (!peek(lx)) break;
            char c = advance(lx);
            if (c == 'n') c = '\n';
            else if (c == 't') c = '\t';
            else if (c == '\\') c = '\\';
            else if (c == '"') c = '"';
            if (len + 1 >= cap) {
                cap *= 2;
                buf = realloc(buf, cap);
            }
            buf[len++] = c;
        } else {
            if (len + 1 >= cap) {
                cap *= 2;
                buf = realloc(buf, cap);
            }
            buf[len++] = advance(lx);
        }
    }
    if (peek(lx) == quote)
        advance(lx);
    buf[len] = '\0';
    return buf;
}

static char *read_word(Lexer *lx) {
    size_t cap = 64, len = 0;
    char *buf = malloc(cap);
    if (!buf) return NULL;

    while (peek(lx)) {
        char c = peek(lx);
        if (c == ' ' || c == '\t' || c == '\n' || c == '|' || c == '&' ||
            c == ';' || c == '<' || c == '>' || c == '(' || c == ')')
            break;
        if (c == '\'' || c == '"') {
            char *inner = read_quoted(lx, c);
            if (!inner) { free(buf); return NULL; }
            size_t ilen = strlen(inner);
            if (len + ilen + 1 >= cap) {
                cap = len + ilen + 2;
                buf = realloc(buf, cap);
            }
            memcpy(buf + len, inner, ilen);
            len += ilen;
            free(inner);
            continue;
        }
        if (c == '\\') {
            advance(lx);
            if (!peek(lx)) break;
            if (len + 1 >= cap) { cap *= 2; buf = realloc(buf, cap); }
            buf[len++] = advance(lx);
            continue;
        }
        if (len + 1 >= cap) { cap *= 2; buf = realloc(buf, cap); }
        buf[len++] = advance(lx);
    }
    buf[len] = '\0';
    return buf;
}

void token_free(Token *tok) {
    if (tok->value) {
        free(tok->value);
        tok->value = NULL;
    }
}

void lexer_init(Lexer *lx, const char *input) {
    lx->input = input;
    lx->pos = 0;
    lx->current.type = TOK_EOF;
    lx->current.value = NULL;
}

void lexer_next(Lexer *lx) {
    token_free(&lx->current);
    skip_ws(lx);

    char c = peek(lx);
    if (!c) {
        lx->current.type = TOK_EOF;
        lx->current.value = NULL;
        return;
    }

    if (c == '#') {
        while (peek(lx) && peek(lx) != '\n')
            advance(lx);
        lexer_next(lx);
        return;
    }

    if (c == '|') {
        advance(lx);
        if (peek(lx) == '|') {
            advance(lx);
            lx->current.type = TOK_OROR;
        } else {
            lx->current.type = TOK_PIPE;
        }
        return;
    }
    if (c == ';') {
        advance(lx);
        lx->current.type = TOK_SEMI;
        return;
    }
    if (c == '<') {
        advance(lx);
        lx->current.type = TOK_LT;
        return;
    }
    if (c == '(') {
        advance(lx);
        lx->current.type = TOK_LPAREN;
        return;
    }
    if (c == ')') {
        advance(lx);
        lx->current.type = TOK_RPAREN;
        return;
    }
    if (c == '&') {
        advance(lx);
        if (peek(lx) == '&') {
            advance(lx);
            lx->current.type = TOK_ANDAND;
        } else if (peek(lx) == '>') {
            advance(lx);
            lx->current.type = TOK_ANDGT;
        } else {
            lx->current.type = TOK_AMP;
        }
        return;
    }
    if (c == '>') {
        advance(lx);
        if (peek(lx) == '>') {
            advance(lx);
            lx->current.type = TOK_GTGT;
        } else {
            lx->current.type = TOK_GT;
        }
        return;
    }
    if (c == '2' && lx->input[lx->pos + 1] == '>') {
        advance(lx);
        advance(lx);
        if (peek(lx) == '>') {
            advance(lx);
            lx->current.type = TOK_2GTGT;
        } else {
            lx->current.type = TOK_2GT;
        }
        return;
    }
    lx->current.type = TOK_WORD;
    lx->current.value = read_word(lx);
    if (!lx->current.value)
        lx->current.type = TOK_ERROR;
}
