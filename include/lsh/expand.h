#ifndef EXPAND_H
#define EXPAND_H

#include <lsh/shell.h>

char* expand_word(Shell* sh, const char* word, int in_double_quotes);
char** expand_argv(Shell* sh, char** argv, int argc, int* out_argc);
void glob_expand(const char* pattern, char*** out, int* out_count);

#endif
