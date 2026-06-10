#include <lsh/shell.h>
#include <lsh/vars.h>
#include <lsh/jobs.h>
#include <lsh/signals.h>

Shell g_shell;

void shell_init(Shell* sh, int interactive) {
    memset(sh, 0, sizeof(*sh));
    sh->interactive = interactive;
    sh->last_status = 0;
    sh->shell_pgid = getpid();
    if (interactive) {
        setpgid(sh->shell_pgid, sh->shell_pgid);
        if (isatty(STDIN_FILENO))
            tcsetpgrp(STDIN_FILENO, sh->shell_pgid);
    }
    vars_init(sh);
    jobs_init(sh);
    signals_setup(sh);
    signals_save_terminal(sh);
}

void shell_destroy(Shell* sh) {
    for (int i = 0; i < sh->history_count; i++)
        free(sh->history[i]);
    vars_destroy(sh);
    for (int i = 0; i < sh->njobs; i++)
        free(sh->jobs[i].cmd);
    signals_restore_terminal(sh);
}
