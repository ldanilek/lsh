#ifndef AST_H
#define AST_H

#include <lsh/shell.h>

typedef enum {
    REDIR_IN,
    REDIR_OUT,
    REDIR_APPEND,
    REDIR_ERR_OUT,
    REDIR_ERR_APPEND,
    REDIR_BOTH_OUT
} RedirType;

typedef struct Redir {
    RedirType type;
    char *file;
    struct Redir *next;
} Redir;

typedef struct Command {
    char **argv;
    int argc;
    Redir *redirs;
    int background;
} Command;

typedef struct Pipeline {
    Command *commands;
    int ncommands;
} Pipeline;

typedef struct AstNode {
    Pipeline pipeline;
    int op; /* 0=none(end), 1=;, 2=&&, 3=|| */
    struct AstNode *next;
} AstNode;

void redir_free(Redir *r);
void command_free(Command *cmd);
void pipeline_free(Pipeline *pl);
void ast_free(AstNode *ast);
AstNode *ast_last(AstNode *ast);

#endif
