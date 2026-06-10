#ifndef COMPLETE_H
#define COMPLETE_H

#include <lsh/shell.h>

typedef struct {
    char** items;
    int count;
} CompletionList;

void completion_free(CompletionList* list);
int completion_gather(Shell* sh, const char* line, int word_start, int word_end,
                      CompletionList* out);
char* completion_common_prefix(CompletionList* list);

#endif
