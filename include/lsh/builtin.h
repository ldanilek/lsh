#ifndef BUILTIN_H
#define BUILTIN_H

#include <lsh/shell.h>

int builtin_is(const char* name);
int builtin_run(Shell* sh, char** argv, int argc);
int builtin_count(void);
const char* builtin_name(int index);

#endif
