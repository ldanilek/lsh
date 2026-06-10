#include <lsh/parser.h>

static Lexer g_lx;
static char g_parse_error[256];

char *parse_error(void) {
    return g_parse_error[0] ? g_parse_error : NULL;
}

static void set_error(const char *msg) {
    snprintf(g_parse_error, sizeof(g_parse_error), "%s", msg);
}

static Redir *parse_redir(void) {
    RedirType type;
    TokenType tt = g_lx.current.type;

    if (tt == TOK_LT) type = REDIR_IN;
    else if (tt == TOK_GT) type = REDIR_OUT;
    else if (tt == TOK_GTGT) type = REDIR_APPEND;
    else if (tt == TOK_2GT) type = REDIR_ERR_OUT;
    else if (tt == TOK_2GTGT) type = REDIR_ERR_APPEND;
    else if (tt == TOK_ANDGT) type = REDIR_BOTH_OUT;
    else return NULL;

    lexer_next(&g_lx);
    if (g_lx.current.type != TOK_WORD) {
        set_error("expected filename after redirect");
        return NULL;
    }

    Redir *r = calloc(1, sizeof(Redir));
    r->type = type;
    r->file = strdup(g_lx.current.value);
    lexer_next(&g_lx);
    return r;
}

static Command *parse_command(int *background) {
    Command cmd;
    memset(&cmd, 0, sizeof(cmd));
    *background = 0;

    size_t acap = 8;
    cmd.argv = calloc(acap, sizeof(char *));
    cmd.argc = 0;

    while (g_lx.current.type == TOK_WORD ||
           g_lx.current.type == TOK_LT ||
           g_lx.current.type == TOK_GT ||
           g_lx.current.type == TOK_GTGT ||
           g_lx.current.type == TOK_2GT ||
           g_lx.current.type == TOK_2GTGT ||
           g_lx.current.type == TOK_ANDGT) {

        if (g_lx.current.type == TOK_WORD) {
            if ((size_t)cmd.argc + 1 >= acap) {
                acap *= 2;
                cmd.argv = realloc(cmd.argv, acap * sizeof(char *));
            }
            cmd.argv[cmd.argc++] = strdup(g_lx.current.value);
            lexer_next(&g_lx);
        } else {
            Redir *r = parse_redir();
            if (!r) return NULL;
            r->next = cmd.redirs;
            cmd.redirs = r;
        }
    }

    if (g_lx.current.type == TOK_AMP) {
        *background = 1;
        lexer_next(&g_lx);
    }

    if (cmd.argc == 0 && !cmd.redirs)
        return NULL;

    Command *out = malloc(sizeof(Command));
    *out = cmd;
    return out;
}

static Pipeline *parse_pipeline(int *background) {
    *background = 0;
    size_t ncap = 4;
    Pipeline *pl = calloc(1, sizeof(Pipeline));
    pl->commands = calloc(ncap, sizeof(Command));

    int bg = 0;
    Command *cmd = parse_command(&bg);
    if (!cmd) {
        if (parse_error()[0])
            return NULL;
        /* empty command is ok in some contexts */
        free(pl->commands);
        free(pl);
        return NULL;
    }
    pl->commands[pl->ncommands++] = *cmd;
    free(cmd);
    if (bg) *background = 1;

    while (g_lx.current.type == TOK_PIPE) {
        lexer_next(&g_lx);
        cmd = parse_command(&bg);
        if (!cmd) {
            set_error("expected command after |");
            pipeline_free(pl);
            free(pl);
            return NULL;
        }
        if ((size_t)pl->ncommands + 1 >= ncap) {
            ncap *= 2;
            pl->commands = realloc(pl->commands, ncap * sizeof(Command));
        }
        pl->commands[pl->ncommands++] = *cmd;
        free(cmd);
        if (bg) *background = 1;
    }

    return pl;
}

AstNode *parse(const char *input) {
    g_parse_error[0] = '\0';
    lexer_init(&g_lx, input);
    lexer_next(&g_lx);

    AstNode *head = NULL;
    AstNode *tail = NULL;

    while (g_lx.current.type != TOK_EOF) {
        int background = 0;
        Pipeline *pl = parse_pipeline(&background);
        if (!pl) {
            if (g_lx.current.type == TOK_EOF)
                break;
            if (!parse_error()[0])
                set_error("syntax error");
            ast_free(head);
            return NULL;
        }

        if (pl->ncommands > 0 && background)
            pl->commands[pl->ncommands - 1].background = 1;

        AstNode *node = calloc(1, sizeof(AstNode));
        node->pipeline = *pl;
        node->op = 0;
        free(pl);

        if (!head) head = node;
        else tail->next = node;
        tail = node;

        if (g_lx.current.type == TOK_SEMI) {
            node->op = 1;
            lexer_next(&g_lx);
        } else if (g_lx.current.type == TOK_ANDAND) {
            node->op = 2;
            lexer_next(&g_lx);
        } else if (g_lx.current.type == TOK_OROR) {
            node->op = 3;
            lexer_next(&g_lx);
        }
    }

    return head;
}
