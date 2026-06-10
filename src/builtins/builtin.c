#include <lsh/builtin.h>
#include <lsh/vars.h>
#include <lsh/jobs.h>
#include <lsh/parser.h>
#include <lsh/execute.h>

static int bi_exit(Shell* sh, char** argv, int argc) {
    (void)argv;
    int code = sh->last_status;
    if (argc > 1)
        code = atoi(argv[1]);
    sh->should_exit = 1;
    sh->last_status = code;
    return code;
}

static int bi_cd(Shell* sh, char** argv, int argc) {
    (void)sh;
    const char* dir;
    if (argc < 2) {
        dir = getenv("HOME");
        if (!dir) {
            fprintf(stderr, "lsh: cd: HOME not set\n");
            return 1;
        }
    } else {
        dir = argv[1];
    }
    if (chdir(dir) < 0) {
        fprintf(stderr, "lsh: cd: %s: %s\n", dir, strerror(errno));
        return 1;
    }
    return 0;
}

static int bi_pwd(Shell* sh, char** argv, int argc) {
    (void)sh;
    (void)argv;
    (void)argc;
    char buf[PATH_MAX];
    if (!getcwd(buf, sizeof(buf))) {
        perror("pwd");
        return 1;
    }
    printf("%s\n", buf);
    return 0;
}

static int bi_echo(Shell* sh, char** argv, int argc) {
    (void)sh;
    for (int i = 1; i < argc; i++) {
        if (i > 1) putchar(' ');
        fputs(argv[i], stdout);
    }
    putchar('\n');
    return 0;
}

static int bi_export(Shell* sh, char** argv, int argc) {
    if (argc == 1) {
        for (int i = 0; i < sh->nvars; i++) {
            if (sh->vars[i].exported)
                printf("export %s=\"%s\"\n", sh->vars[i].name, sh->vars[i].value);
        }
        return 0;
    }
    for (int i = 1; i < argc; i++) {
        char* eq = strchr(argv[i], '=');
        if (eq) {
            char name[256];
            size_t nlen = (size_t)(eq - argv[i]);
            if (nlen >= sizeof(name)) return 1;
            memcpy(name, argv[i], nlen);
            name[nlen] = '\0';
            var_set(sh, name, eq + 1, 1);
        } else {
            Var* v = NULL;
            for (int j = 0; j < sh->nvars; j++) {
                if (strcmp(sh->vars[j].name, argv[i]) == 0) {
                    v = &sh->vars[j];
                    break;
                }
            }
            if (v) {
                v->exported = 1;
                setenv(v->name, v->value, 1);
            } else {
                var_set(sh, argv[i], "", 1);
            }
        }
    }
    return 0;
}

static int bi_unset(Shell* sh, char** argv, int argc) {
    for (int i = 1; i < argc; i++)
        var_unset(sh, argv[i]);
    return 0;
}

static int bi_true(Shell* sh, char** argv, int argc) {
    (void)sh;
    (void)argv;
    (void)argc;
    return 0;
}

static int bi_false(Shell* sh, char** argv, int argc) {
    (void)sh;
    (void)argv;
    (void)argc;
    return 1;
}

static int bi_colon(Shell* sh, char** argv, int argc) {
    return bi_true(sh, argv, argc);
}

static int bi_type(Shell* sh, char** argv, int argc) {
    (void)sh;
    if (argc < 2) return 1;
    if (builtin_is(argv[1]))
        printf("%s is a shell builtin\n", argv[1]);
    else
        printf("%s is external\n", argv[1]);
    return 0;
}

static int bi_kill(Shell* sh, char** argv, int argc) {
    (void)sh;
    if (argc < 2) {
        fprintf(stderr, "kill: usage: kill <pid>\n");
        return 1;
    }
    pid_t pid = (pid_t)atoi(argv[1]);
    if (kill(pid, SIGTERM) < 0) {
        fprintf(stderr, "kill: %s\n", strerror(errno));
        return 1;
    }
    return 0;
}

static int bi_history(Shell* sh, char** argv, int argc) {
    (void)argv;
    (void)argc;
    for (int i = 0; i < sh->history_count; i++)
        printf("%4d  %s\n", i + 1, sh->history[i]);
    return 0;
}

static int bi_source(Shell* sh, char** argv, int argc) {
    if (argc < 2) {
        fprintf(stderr, "source: filename required\n");
        return 1;
    }
    FILE* f = fopen(argv[1], "r");
    if (!f) {
        fprintf(stderr, "source: %s: %s\n", argv[1], strerror(errno));
        return 1;
    }
    char line[LSH_MAX_LINE];
    int status = 0;
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';
        if (len == 0) continue;
        AstNode* ast = parse(line);
        if (!ast) {
            char* err = parse_error();
            if (err) fprintf(stderr, "lsh: %s\n", err);
            status = 2;
            continue;
        }
        int s = execute_ast(sh, ast);
        ast_free(ast);
        if (sh->should_exit) break;
        status = s;
    }
    fclose(f);
    return status;
}

typedef struct {
    const char* name;
    int (*fn)(Shell*, char**, int);
} BuiltinEntry;

static BuiltinEntry builtins[] = {
    {"exit", bi_exit},
    {"cd", bi_cd},
    {"pwd", bi_pwd},
    {"echo", bi_echo},
    {"export", bi_export},
    {"unset", bi_unset},
    {"true", bi_true},
    {"false", bi_false},
    {":", bi_colon},
    {"type", bi_type},
    {"kill", bi_kill},
    {"history", bi_history},
    {"source", bi_source},
    {".", bi_source},
    {NULL, NULL}};

int builtin_count(void) {
    int n = 0;
    while (builtins[n].name) n++;
    return n;
}

const char* builtin_name(int index) {
    if (index < 0 || index >= builtin_count()) return NULL;
    return builtins[index].name;
}

int builtin_is(const char* name) {
    for (int i = 0; builtins[i].name; i++) {
        if (strcmp(builtins[i].name, name) == 0)
            return 1;
    }
    return 0;
}

int builtin_run(Shell* sh, char** argv, int argc) {
    for (int i = 0; builtins[i].name; i++) {
        if (strcmp(builtins[i].name, argv[0]) == 0)
            return builtins[i].fn(sh, argv, argc);
    }
    return -1;
}
