#ifndef SHELL_H
#define SHELL_H

#define _POSIX_C_SOURCE 200809L

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <termios.h>
#include <limits.h>
#include <dirent.h>
#include <pwd.h>
#include <ctype.h>
#include <stdbool.h>

#define LSH_MAX_LINE   4096
#define LSH_MAX_ARGS   256
#define LSH_MAX_JOBS   64
#define LSH_MAX_VARS   512
#define LSH_MAX_HISTORY 256
#define LSH_PROMPT     "lsh$ "

typedef struct Var {
    char *name;
    char *value;
    int exported;
} Var;

typedef struct Job {
    int id;
    pid_t pgid;
    pid_t pids[LSH_MAX_ARGS];
    int npids;
    char *cmd;
    int running; /* 1=running, 0=done */
    int stopped;
} Job;

typedef struct Shell {
    int last_status;
    int interactive;
    int should_exit;
    pid_t shell_pgid;
    pid_t last_bg_pid;
    int last_job_id;

    Var vars[LSH_MAX_VARS];
    int nvars;

    Job jobs[LSH_MAX_JOBS];
    int njobs;

    char *history[LSH_MAX_HISTORY];
    int history_count;
    int history_pos;

    struct termios shell_termios;
    int shell_termios_saved;

    pid_t fg_pgid;
} Shell;

extern Shell g_shell;

void shell_init(Shell *sh, int interactive);
void shell_destroy(Shell *sh);

#endif
