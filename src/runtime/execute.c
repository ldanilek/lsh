#include <lsh/execute.h>
#include <lsh/expand.h>
#include <lsh/builtin.h>
#include <lsh/vars.h>
#include <lsh/jobs.h>
#include <lsh/signals.h>

static int is_assign_word(const char *s) {
    if (!s || !s[0] || isdigit((unsigned char)s[0])) return 0;
    char *eq = strchr(s, '=');
    if (!eq || eq == s) return 0;
    for (const char *p = s; p < eq; p++) {
        if (!isalnum((unsigned char)*p) && *p != '_')
            return 0;
    }
    for (const char *p = eq + 1; *p; p++) {
        if (*p == ';' || *p == '|' || *p == '&')
            return 0;
    }
    return 1;
}

static int apply_redirs(Redir *r) {
    while (r) {
        int fd;
        switch (r->type) {
        case REDIR_IN:
            fd = open(r->file, O_RDONLY);
            if (fd < 0) { perror(r->file); return -1; }
            dup2(fd, STDIN_FILENO);
            close(fd);
            break;
        case REDIR_OUT:
            fd = open(r->file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) { perror(r->file); return -1; }
            dup2(fd, STDOUT_FILENO);
            close(fd);
            break;
        case REDIR_APPEND:
            fd = open(r->file, O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (fd < 0) { perror(r->file); return -1; }
            dup2(fd, STDOUT_FILENO);
            close(fd);
            break;
        case REDIR_ERR_OUT:
            fd = open(r->file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) { perror(r->file); return -1; }
            dup2(fd, STDERR_FILENO);
            close(fd);
            break;
        case REDIR_ERR_APPEND:
            fd = open(r->file, O_WRONLY | O_CREAT | O_APPEND, 0644);
            if (fd < 0) { perror(r->file); return -1; }
            dup2(fd, STDERR_FILENO);
            close(fd);
            break;
        case REDIR_BOTH_OUT:
            fd = open(r->file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd < 0) { perror(r->file); return -1; }
            dup2(fd, STDOUT_FILENO);
            dup2(fd, STDERR_FILENO);
            close(fd);
            break;
        }
        r = r->next;
    }
    return 0;
}

static char *join_cmd(Command *cmds, int n) {
    size_t len = 0;
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < cmds[i].argc; j++)
            len += strlen(cmds[i].argv[j]) + 1;
        len += 4;
    }
    char *s = malloc(len + 1);
    s[0] = '\0';
    for (int i = 0; i < n; i++) {
        if (i > 0) strcat(s, " | ");
        for (int j = 0; j < cmds[i].argc; j++) {
            if (j > 0) strcat(s, " ");
            strcat(s, cmds[i].argv[j]);
        }
    }
    return s;
}

static int run_builtin_cmd(Shell *sh, char **argv, int argc, Redir *redirs) {
    int saved_in = -1, saved_out = -1, saved_err = -1;
    if (redirs) {
        saved_in = dup(STDIN_FILENO);
        saved_out = dup(STDOUT_FILENO);
        saved_err = dup(STDERR_FILENO);
        if (apply_redirs(redirs) < 0) {
            if (saved_in >= 0) close(saved_in);
            if (saved_out >= 0) close(saved_out);
            if (saved_err >= 0) close(saved_err);
            return 1;
        }
    }
    int status = builtin_run(sh, argv, argc);
    if (redirs) {
        fflush(stdout);
        fflush(stderr);
        if (saved_in >= 0) { dup2(saved_in, STDIN_FILENO); close(saved_in); }
        if (saved_out >= 0) { dup2(saved_out, STDOUT_FILENO); close(saved_out); }
        if (saved_err >= 0) { dup2(saved_err, STDERR_FILENO); close(saved_err); }
    }
    return status;
}

int execute_pipeline(Shell *sh, Pipeline *pl) {
    if (!pl || pl->ncommands == 0)
        return 0;

    int n = pl->ncommands;
    int background = pl->commands[n - 1].background;
    char *cmd_str = join_cmd(pl->commands, n);

    if (n == 1 && pl->commands[0].argc == 0) {
        free(cmd_str);
        return 0;
    }

    /* Single builtin without pipe */
    if (n == 1) {
        Command *c = &pl->commands[0];
        int argc;
        char **argv = expand_argv(sh, c->argv, c->argc, &argc);
        if (argc == 0) { free(argv); free(cmd_str); return 0; }

        if (argc == 1 && is_assign_word(argv[0])) {
            var_assign_str(sh, argv[0]);
            free(argv[0]);
            free(argv);
            free(cmd_str);
            return 0;
        }

        if (strcmp(argv[0], "jobs") == 0) {
            int s = jobs_builtin(sh, argv, argc);
            for (int i = 0; i < argc; i++) free(argv[i]);
            free(argv); free(cmd_str);
            return s;
        }
        if (strcmp(argv[0], "fg") == 0) {
            int s = fg_builtin(sh, argv, argc);
            for (int i = 0; i < argc; i++) free(argv[i]);
            free(argv); free(cmd_str);
            return s;
        }
        if (strcmp(argv[0], "bg") == 0) {
            int s = bg_builtin(sh, argv, argc);
            for (int i = 0; i < argc; i++) free(argv[i]);
            free(argv); free(cmd_str);
            return s;
        }

        if (builtin_is(argv[0]) && !background) {
            int s = run_builtin_cmd(sh, argv, argc, c->redirs);
            for (int i = 0; i < argc; i++) free(argv[i]);
            free(argv); free(cmd_str);
            return s;
        }
        for (int i = 0; i < argc; i++) free(argv[i]);
        free(argv);
    }

    int pipes[2];
    pid_t pids[LSH_MAX_ARGS];
    int npids = 0;
    pid_t pgid = 0;
    int in_fd = -1;
    int syncfd[2] = {-1, -1};
    int need_tty_sync = !background && sh->interactive;

    if (need_tty_sync && pipe(syncfd) < 0) {
        perror("pipe");
        free(cmd_str);
        return 1;
    }

    for (int i = 0; i < n; i++) {
        Command *c = &pl->commands[i];
        int argc;
        char **argv = expand_argv(sh, c->argv, c->argc, &argc);
        if (argc == 0) {
            free(argv);
            continue;
        }

        int out_fd = -1;
        if (i < n - 1) {
            if (pipe(pipes) < 0) { perror("pipe"); return 1; }
            out_fd = pipes[1];
        }

        pid_t pid = fork();
        if (pid < 0) { perror("fork"); return 1; }

        if (pid == 0) {
            if (pgid == 0) setpgid(0, 0);
            else setpgid(0, pgid);

            if (in_fd >= 0) {
                dup2(in_fd, STDIN_FILENO);
                close(in_fd);
            }
            if (out_fd >= 0) {
                dup2(out_fd, STDOUT_FILENO);
                close(out_fd);
            }
            if (i < n - 1)
                close(pipes[0]);

            if (apply_redirs(c->redirs) < 0)
                _exit(1);

            if (builtin_is(argv[0])) {
                int s = builtin_run(sh, argv, argc);
                fflush(stdout);
                fflush(stderr);
                _exit(s);
            }

            if (need_tty_sync) {
                if (i == n - 1) {
                    close(syncfd[1]);
                    char byte;
                    if (read(syncfd[0], &byte, 1) != 1)
                        _exit(1);
                    close(syncfd[0]);
                } else {
                    close(syncfd[0]);
                    close(syncfd[1]);
                }
            }

            signals_reset_for_exec();
            if (need_tty_sync && i == n - 1)
                signals_child_prepare_tty();
            execvp(argv[0], argv);
            fprintf(stderr, "lsh: %s: command not found\n", argv[0]);
            _exit(127);
        }

        if (pgid == 0) pgid = pid;
        else setpgid(pid, pgid);

        pids[npids++] = pid;

        if (need_tty_sync && i == n - 1) {
            close(syncfd[0]);
            signals_handoff_foreground(sh, pgid);
            char byte = 'x';
            if (write(syncfd[1], &byte, 1) != 1) {
                perror("write");
                kill(pid, SIGTERM);
                free(cmd_str);
                return 1;
            }
            close(syncfd[1]);
            syncfd[0] = syncfd[1] = -1;
        }

        if (in_fd >= 0) close(in_fd);
        if (i < n - 1) {
            close(out_fd);
            in_fd = pipes[0];
        }

        for (int j = 0; j < argc; j++) free(argv[j]);
        free(argv);
    }

    if (in_fd >= 0) close(in_fd);
    if (syncfd[0] >= 0) close(syncfd[0]);
    if (syncfd[1] >= 0) close(syncfd[1]);

    int status = 0;
    if (background) {
        jobs_add(sh, pgid, pids, npids, cmd_str, 1);
        free(cmd_str);
        return 0;
    }

    int wait_flags = sh->interactive ? WUNTRACED : 0;
    if (!sh->interactive)
        signals_set_foreground(sh, pgid);
    for (int i = 0; i < npids; i++) {
        int s = 0;
        int got_status = 0;
        for (;;) {
            if (waitpid(pids[i], &s, wait_flags) < 0)
                break;
            got_status = 1;
            if (WIFSTOPPED(s) && sh->interactive) {
                int sig = WSTOPSIG(s);
                if (sig == SIGTTIN || sig == SIGTTOU) {
                    signals_handoff_foreground(sh, pgid);
                    kill(-pgid, SIGCONT);
                    continue;
                }
                jobs_add(sh, pgid, pids, npids, cmd_str, 1);
                status = 128 + sig;
                signals_reclaim_foreground(sh);
                free(cmd_str);
                return status;
            }
            break;
        }
        if (!got_status) continue;
        if (WIFEXITED(s)) status = WEXITSTATUS(s);
        else if (WIFSIGNALED(s)) status = 128 + WTERMSIG(s);
    }
    signals_reclaim_foreground(sh);
    free(cmd_str);
    return status;
}

int execute_ast(Shell *sh, AstNode *ast) {
    int status = 0;
    while (ast) {
        status = execute_pipeline(sh, &ast->pipeline);
        sh->last_status = status;

        if (!ast->next) break;

        if (ast->op == 2 && status != 0)
            break;
        if (ast->op == 3 && status == 0)
            break;

        ast = ast->next;
    }
    return status;
}
