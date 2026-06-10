#include <lsh/signals.h>
#include <lsh/jobs.h>

static Shell* g_sh;

static void sigint_handler(int sig) {
    (void)sig;
    if (g_sh && g_sh->fg_pgid > 0)
        kill(-g_sh->fg_pgid, SIGINT);
    else if (g_sh && g_sh->interactive)
        write(STDOUT_FILENO, "\n", 1);
}

static void sigchld_handler(int sig) {
    (void)sig;
    if (g_sh)
        jobs_reap(g_sh);
}

void signals_setup(Shell* sh) {
    g_sh = sh;
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, NULL);

    sa.sa_handler = sigchld_handler;
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);

    sa.sa_handler = SIG_IGN;
    sigaction(SIGQUIT, &sa, NULL);
    sigaction(SIGTTOU, &sa, NULL);
    sigaction(SIGTTIN, &sa, NULL);
}

void signals_save_terminal(Shell* sh) {
    if (!sh->interactive) return;
    if (tcgetattr(STDIN_FILENO, &sh->shell_termios) == 0)
        sh->shell_termios_saved = 1;
}

void signals_restore_terminal(Shell* sh) {
    if (sh->shell_termios_saved)
        tcsetattr(STDIN_FILENO, TCSADRAIN, &sh->shell_termios);
}

void signals_set_foreground(Shell* sh, pid_t pgid) {
    sh->fg_pgid = pgid;
    if (!sh->interactive || !isatty(STDIN_FILENO)) return;
    pid_t target = pgid > 0 ? pgid : sh->shell_pgid;
    while (tcsetpgrp(STDIN_FILENO, target) < 0 && errno == EINTR);
}

void signals_reset_for_exec(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    const int sigs[] = {
        SIGINT, SIGQUIT, SIGTSTP, SIGTTIN, SIGTTOU, SIGCHLD, 0};
    for (int i = 0; sigs[i]; i++)
        sigaction(sigs[i], &sa, NULL);
}

void signals_handoff_foreground(Shell* sh, pid_t pgid) {
    if (!sh->interactive || pgid <= 0) return;
    signals_restore_terminal(sh);
    if (isatty(STDIN_FILENO))
        tcflush(STDIN_FILENO, TCIFLUSH);
    signals_set_foreground(sh, pgid);
}

void signals_child_prepare_tty(void) {
    int ttyfd = open("/dev/tty", O_RDWR | O_CLOEXEC);
    if (ttyfd >= 0) {
        dup2(ttyfd, STDIN_FILENO);
        dup2(ttyfd, STDOUT_FILENO);
        dup2(ttyfd, STDERR_FILENO);
        close(ttyfd);
    }
    if (!isatty(STDIN_FILENO))
        return;
    pid_t pgrp = getpgrp();
    while (tcsetpgrp(STDIN_FILENO, pgrp) < 0 && errno == EINTR);
    tcflush(STDIN_FILENO, TCIOFLUSH);
}

void signals_reclaim_foreground(Shell* sh) {
    if (!sh->interactive) {
        sh->fg_pgid = 0;
        return;
    }
    signals_set_foreground(sh, 0);
    signals_restore_terminal(sh);
}
