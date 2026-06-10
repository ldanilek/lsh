#include <lsh/shell.h>
#include <lsh/parser.h>
#include <lsh/execute.h>
#include <lsh/input.h>
#include <lsh/vars.h>
#include <lsh/jobs.h>

static int is_assignment(const char* s) {
    if (!s || !s[0]) return 0;
    if (isdigit((unsigned char)s[0])) return 0;
    char* eq = strchr(s, '=');
    if (!eq || eq == s) return 0;
    for (const char* p = s; p < eq; p++) {
        if (!isalnum((unsigned char)*p) && *p != '_')
            return 0;
    }
    for (const char* p = eq + 1; *p; p++) {
        if (*p == ';' || *p == '|' || *p == '&')
            return 0;
    }
    return 1;
}

static int handle_line(Shell* sh, char* line) {
    if (!line || !line[0])
        return 0;

    char* work = strdup(line);
    char* p = work;
    while (*p == ' ' || *p == '\t') p++;

    char saved[LSH_MAX_LINE];
    int had_assign = 0;
    char* cmd_start = NULL;
    while (1) {
        char* tok_start = p;
        while (*p && *p != ' ' && *p != '\t' && *p != ';') p++;
        char saved_ch = *p;
        *p = '\0';
        if (!is_assignment(tok_start)) {
            *p = saved_ch;
            cmd_start = tok_start;
            break;
        }
        var_assign_str(sh, tok_start);
        had_assign = 1;
        *p = saved_ch;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == ';') {
            p++;
            while (*p == ' ' || *p == '\t') p++;
        }
        if (!*p) {
            free(work);
            return 0;
        }
    }

    const char* cmd_line = line;
    if (had_assign && cmd_start) {
        snprintf(saved, sizeof(saved), "%s", cmd_start);
        cmd_line = saved;
    }

    AstNode* ast = parse(cmd_line);
    free(work);

    if (!ast) {
        char* err = parse_error();
        if (err && err[0]) {
            fprintf(stderr, "lsh: %s\n", err);
            return 2;
        }
        return 0;
    }

    int status = execute_ast(sh, ast);
    ast_free(ast);
    jobs_reap(sh);
    return status;
}

static int run_script(Shell* sh, const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "lsh: %s: %s\n", path, strerror(errno));
        return 127;
    }
    char line[LSH_MAX_LINE];
    int status = 0;
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';
        status = handle_line(sh, line);
        if (sh->should_exit)
            break;
    }
    fclose(f);
    return sh->should_exit ? sh->last_status : status;
}

static void run_interactive(Shell* sh) {
    while (!sh->should_exit) {
        const char* prompt = var_get(sh, "PS1");
        if (!prompt) prompt = LSH_PROMPT;

        char* line = input_read_line(sh, prompt);
        if (!line)
            break;

        if (line[0])
            history_add(sh, line);

        sh->last_status = handle_line(sh, line);
        free(line);
    }
}

static void usage(const char* prog) {
    fprintf(stderr, "Usage: %s [-c command] [script]\n", prog);
}

int main(int argc, char** argv) {
    int interactive = isatty(STDIN_FILENO);
    shell_init(&g_shell, interactive);

    if (argc > 1 && strcmp(argv[1], "-c") == 0) {
        if (argc < 3) {
            usage(argv[0]);
            shell_destroy(&g_shell);
            return 1;
        }
        g_shell.interactive = 0;
        g_shell.last_status = handle_line(&g_shell, argv[2]);
        jobs_wait_all(&g_shell);
        int code = g_shell.should_exit ? g_shell.last_status : g_shell.last_status;
        shell_destroy(&g_shell);
        return code;
    }

    if (argc > 1) {
        g_shell.interactive = 0;
        int code = run_script(&g_shell, argv[1]);
        jobs_wait_all(&g_shell);
        shell_destroy(&g_shell);
        return code;
    }

    run_interactive(&g_shell);
    int code = g_shell.last_status;
    shell_destroy(&g_shell);
    return code;
}
