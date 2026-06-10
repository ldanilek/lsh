#include <lsh/ast.h>

void redir_free(Redir* r) {
    while (r) {
        Redir* next = r->next;
        free(r->file);
        free(r);
        r = next;
    }
}

void command_free(Command* cmd) {
    if (!cmd) return;
    for (int i = 0; i < cmd->argc; i++)
        free(cmd->argv[i]);
    free(cmd->argv);
    redir_free(cmd->redirs);
}

void pipeline_free(Pipeline* pl) {
    if (!pl) return;
    for (int i = 0; i < pl->ncommands; i++)
        command_free(&pl->commands[i]);
    free(pl->commands);
}

void ast_free(AstNode* ast) {
    while (ast) {
        AstNode* next = ast->next;
        pipeline_free(&ast->pipeline);
        free(ast);
        ast = next;
    }
}

AstNode* ast_last(AstNode* ast) {
    while (ast && ast->next)
        ast = ast->next;
    return ast;
}
