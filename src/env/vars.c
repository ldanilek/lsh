#include <lsh/vars.h>

extern char** environ;

void vars_init(Shell* sh) {
    sh->nvars = 0;
    var_set(sh, "IFS", " \t\n", 1);
    var_set(sh, "PS1", LSH_PROMPT, 0);
}

void vars_destroy(Shell* sh) {
    for (int i = 0; i < sh->nvars; i++) {
        free(sh->vars[i].name);
        free(sh->vars[i].value);
    }
    sh->nvars = 0;
}

static Var* var_find(Shell* sh, const char* name) {
    for (int i = 0; i < sh->nvars; i++) {
        if (strcmp(sh->vars[i].name, name) == 0)
            return &sh->vars[i];
    }
    return NULL;
}

const char* var_get(Shell* sh, const char* name) {
    Var* v = var_find(sh, name);
    if (v) return v->value;
    return getenv(name);
}

int var_set(Shell* sh, const char* name, const char* value, int exported) {
    Var* v = var_find(sh, name);
    if (v) {
        free(v->value);
        v->value = strdup(value ? value : "");
        if (exported) v->exported = 1;
    } else {
        if (sh->nvars >= LSH_MAX_VARS)
            return -1;
        v = &sh->vars[sh->nvars++];
        v->name = strdup(name);
        v->value = strdup(value ? value : "");
        v->exported = exported;
    }
    if (v->exported)
        setenv(name, v->value, 1);
    return 0;
}

int var_unset(Shell* sh, const char* name) {
    for (int i = 0; i < sh->nvars; i++) {
        if (strcmp(sh->vars[i].name, name) == 0) {
            if (sh->vars[i].exported)
                unsetenv(name);
            free(sh->vars[i].name);
            free(sh->vars[i].value);
            sh->vars[i] = sh->vars[--sh->nvars];
            return 0;
        }
    }
    unsetenv(name);
    return 0;
}

char** vars_environ(Shell* sh) {
    (void)sh;
    return environ;
}

int var_assign_str(Shell* sh, const char* assignment) {
    char* eq = strchr(assignment, '=');
    if (!eq) return -1;
    char name[256];
    size_t nlen = (size_t)(eq - assignment);
    if (nlen >= sizeof(name)) return -1;
    memcpy(name, assignment, nlen);
    name[nlen] = '\0';
    return var_set(sh, name, eq + 1, 0);
}
