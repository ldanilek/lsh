#ifndef EXECUTE_H
#define EXECUTE_H

#include <lsh/ast.h>

int execute_ast(Shell* sh, AstNode* ast);
int execute_pipeline(Shell* sh, Pipeline* pl);

#endif
