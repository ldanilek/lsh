#include <lsh/jobs.h>
#include <lsh/signals.h>

void jobs_init(Shell* sh) {
    (void)sh;
}

void jobs_wait_all(Shell* sh) {
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, 0)) > 0) {
        for (int i = 0; i < sh->njobs; i++) {
            if (sh->jobs[i].pgid == pid || sh->jobs[i].pids[sh->jobs[i].npids - 1] == pid)
                sh->jobs[i].running = 0;
        }
    }
    sh->njobs = 0;
}

void jobs_reap(Shell* sh) {
    int status;
    for (int i = 0; i < sh->njobs; i++) {
        Job* j = &sh->jobs[i];
        for (int k = 0; k < j->npids; k++) {
            pid_t pid = waitpid(j->pids[k], &status, WNOHANG | WUNTRACED);
            if (pid <= 0)
                continue;
            if (WIFSTOPPED(status)) {
                j->stopped = 1;
                j->running = 0;
                fprintf(stderr, "\n[%d]+ Stopped\t%s\n", j->id, j->cmd);
            } else {
                j->running = 0;
            }
        }
    }
    for (int i = 0; i < sh->njobs; i++) {
        if (!sh->jobs[i].running && !sh->jobs[i].stopped)
            jobs_remove(sh, sh->jobs[i].id);
    }
}

int jobs_add(Shell* sh, pid_t pgid, pid_t* pids, int npids, const char* cmd, int background) {
    if (!background) return 0;
    if (sh->njobs >= LSH_MAX_JOBS) return -1;

    sh->last_job_id++;
    Job* j = &sh->jobs[sh->njobs++];
    j->id = sh->last_job_id;
    j->pgid = pgid;
    j->npids = npids < LSH_MAX_ARGS ? npids : LSH_MAX_ARGS;
    memcpy(j->pids, pids, (size_t)j->npids * sizeof(pid_t));
    j->cmd = strdup(cmd);
    j->running = 1;
    j->stopped = 0;
    fprintf(stderr, "[%d] %d\n", j->id, (int)pids[npids - 1]);
    sh->last_bg_pid = pids[npids - 1];
    return j->id;
}

Job* jobs_find(Shell* sh, int id) {
    for (int i = 0; i < sh->njobs; i++) {
        if (sh->jobs[i].id == id)
            return &sh->jobs[i];
    }
    return NULL;
}

Job* jobs_find_pgid(Shell* sh, pid_t pgid) {
    for (int i = 0; i < sh->njobs; i++) {
        if (sh->jobs[i].pgid == pgid)
            return &sh->jobs[i];
    }
    return NULL;
}

void jobs_remove(Shell* sh, int id) {
    for (int i = 0; i < sh->njobs; i++) {
        if (sh->jobs[i].id == id) {
            free(sh->jobs[i].cmd);
            sh->jobs[i] = sh->jobs[--sh->njobs];
            return;
        }
    }
}

int jobs_builtin(Shell* sh, char** argv, int argc) {
    (void)argv;
    (void)argc;
    jobs_reap(sh);
    for (int i = 0; i < sh->njobs; i++) {
        Job* j = &sh->jobs[i];
        const char* state = j->stopped ? "Stopped" : "Running";
        fprintf(stderr, "[%d] %s %s\n", j->id, state, j->cmd);
    }
    return 0;
}

static int wait_for_job(Shell* sh, Job* j) {
    signals_handoff_foreground(sh, j->pgid);
    int status = 0;
    for (int i = 0; i < j->npids; i++) {
        int s;
        if (waitpid(j->pids[i], &s, WUNTRACED) < 0) continue;
        if (WIFSTOPPED(s)) {
            j->stopped = 1;
            j->running = 0;
            signals_reclaim_foreground(sh);
            return 128 + WSTOPSIG(s);
        }
        if (WIFEXITED(s))
            status = WEXITSTATUS(s);
        else if (WIFSIGNALED(s))
            status = 128 + WTERMSIG(s);
    }
    j->running = 0;
    jobs_remove(sh, j->id);
    signals_reclaim_foreground(sh);
    return status;
}

int fg_builtin(Shell* sh, char** argv, int argc) {
    jobs_reap(sh);
    Job* j = NULL;
    if (argc < 2) {
        if (sh->njobs == 0) {
            fprintf(stderr, "fg: no current job\n");
            return 1;
        }
        j = &sh->jobs[sh->njobs - 1];
    } else {
        int id = atoi(argv[1]);
        j = jobs_find(sh, id);
        if (!j) {
            fprintf(stderr, "fg: job %d not found\n", id);
            return 1;
        }
    }
    if (j->stopped)
        kill(-j->pgid, SIGCONT);
    fprintf(stderr, "%s\n", j->cmd);
    sh->last_status = wait_for_job(sh, j);
    return sh->last_status;
}

int bg_builtin(Shell* sh, char** argv, int argc) {
    jobs_reap(sh);
    Job* j = NULL;
    if (argc < 2) {
        if (sh->njobs == 0) {
            fprintf(stderr, "bg: no current job\n");
            return 1;
        }
        j = &sh->jobs[sh->njobs - 1];
    } else {
        j = jobs_find(sh, atoi(argv[1]));
        if (!j) {
            fprintf(stderr, "bg: job not found\n");
            return 1;
        }
    }
    if (!j->stopped) {
        fprintf(stderr, "bg: job already running\n");
        return 1;
    }
    kill(-j->pgid, SIGCONT);
    j->stopped = 0;
    j->running = 1;
    fprintf(stderr, "[%d]+ %s &\n", j->id, j->cmd);
    return 0;
}
