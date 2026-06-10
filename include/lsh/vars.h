#ifndef VARS_H
#define VARS_H

#include <lsh/shell.h>

void vars_init(Shell *sh);
void vars_destroy(Shell *sh);
const char *var_get(Shell *sh, const char *name);
int var_set(Shell *sh, const char *name, const char *value, int exported);
int var_unset(Shell *sh, const char *name);
char **vars_environ(Shell *sh);
int var_assign_str(Shell *sh, const char *assignment);

#endif
