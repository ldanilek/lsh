#ifndef JOBS_H
#define JOBS_H

#include <lsh/shell.h>

void jobs_init(Shell* sh);
void jobs_reap(Shell* sh);
void jobs_wait_all(Shell* sh);
int jobs_add(Shell* sh, pid_t pgid, pid_t* pids, int npids, const char* cmd, int background);
Job* jobs_find(Shell* sh, int id);
Job* jobs_find_pgid(Shell* sh, pid_t pgid);
void jobs_remove(Shell* sh, int id);
int jobs_builtin(Shell* sh, char** argv, int argc);
int fg_builtin(Shell* sh, char** argv, int argc);
int bg_builtin(Shell* sh, char** argv, int argc);

#endif
