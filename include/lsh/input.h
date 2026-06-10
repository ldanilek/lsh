#ifndef INPUT_H
#define INPUT_H

#include <lsh/shell.h>

char *input_read_line(Shell *sh, const char *prompt);
void history_add(Shell *sh, const char *line);

#endif
