#include <lsh/expand.h>
#include <lsh/vars.h>

static int match_pattern(const char* pat, const char* str) {
    if (!*pat) return !*str;
    if (*pat == '*') {
        if (match_pattern(pat + 1, str)) return 1;
        if (*str && match_pattern(pat, str + 1)) return 1;
        return 0;
    }
    if (*pat == '?')
        return *str && match_pattern(pat + 1, str + 1);
    if (*pat == *str)
        return *str && match_pattern(pat + 1, str + 1);
    return 0;
}

void glob_expand(const char* pattern, char*** out, int* out_count) {
    *out = NULL;
    *out_count = 0;

    if (!strchr(pattern, '*') && !strchr(pattern, '?')) {
        *out = malloc(sizeof(char*));
        (*out)[0] = strdup(pattern);
        *out_count = 1;
        return;
    }

    char dir[PATH_MAX];
    const char* base = strrchr(pattern, '/');
    if (base) {
        size_t dlen = (size_t)(base - pattern);
        if (dlen >= sizeof(dir)) dlen = sizeof(dir) - 1;
        memcpy(dir, pattern, dlen);
        dir[dlen] = '\0';
        base++;
    } else {
        strcpy(dir, ".");
        base = pattern;
    }

    DIR* d = opendir(dir[0] ? dir : ".");
    if (!d) {
        *out = malloc(sizeof(char*));
        (*out)[0] = strdup(pattern);
        *out_count = 1;
        return;
    }

    size_t cap = 8;
    *out = malloc(cap * sizeof(char*));
    struct dirent* ent;
    while ((ent = readdir(d))) {
        if (ent->d_name[0] == '.' && base[0] != '.')
            continue;
        if (!match_pattern(base, ent->d_name))
            continue;
        char path[PATH_MAX];
        if (strcmp(dir, ".") == 0)
            snprintf(path, sizeof(path), "%s", ent->d_name);
        else
            snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name);

        if ((size_t)*out_count + 1 >= cap) {
            cap *= 2;
            *out = realloc(*out, cap * sizeof(char*));
        }
        (*out)[(*out_count)++] = strdup(path);
    }
    closedir(d);

    if (*out_count == 0) {
        *out = realloc(*out, sizeof(char*));
        (*out)[0] = strdup(pattern);
        *out_count = 1;
    }
}

static char* expand_special(Shell* sh, const char* name) {
    if (strcmp(name, "?") == 0) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", sh->last_status);
        return strdup(buf);
    }
    if (strcmp(name, "$") == 0 || strcmp(name, "PPID") == 0) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", (int)getpid());
        return strdup(buf);
    }
    if (strcmp(name, "!") == 0) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", (int)sh->last_bg_pid);
        return strdup(buf);
    }
    if (strcmp(name, "#") == 0)
        return strdup("0");
    const char* val = var_get(sh, name);
    return strdup(val ? val : "");
}

static char* expand_tilde(Shell* sh, const char* word) {
    (void)sh;
    if (word[0] != '~') return strdup(word);
    const char* home = getenv("HOME");
    if (!home) {
        struct passwd* pw = getpwuid(getuid());
        home = pw ? pw->pw_dir : "";
    }
    if (word[1] == '\0' || word[1] == '/')
        return strdup(home ? home : word);
    return strdup(word);
}

char* expand_word(Shell* sh, const char* word, int in_double_quotes) {
    if (!word) return strdup("");

    char* result = expand_tilde(sh, word);
    if (strchr(result, '$') == NULL && !in_double_quotes &&
        (strchr(result, '*') || strchr(result, '?'))) {
        char** globs;
        int gc;
        glob_expand(result, &globs, &gc);
        free(result);
        if (gc == 1) return globs[0];
        /* multiple globs joined with space for simple cases */
        size_t total = 0;
        for (int i = 0; i < gc; i++) total += strlen(globs[i]) + 1;
        result = malloc(total + 1);
        result[0] = '\0';
        for (int i = 0; i < gc; i++) {
            if (i > 0) strcat(result, " ");
            strcat(result, globs[i]);
            free(globs[i]);
        }
        free(globs);
        return result;
    }

    size_t cap = strlen(result) + 1, len = 0;
    char* out = malloc(cap);
    out[0] = '\0';

    const char* p = result;
    while (*p) {
        if (*p == '$' && !in_double_quotes) {
            p++;
            char name[256];
            int ni = 0;
            if (*p == '{') {
                p++;
                while (*p && *p != '}' && ni < (int)sizeof(name) - 1)
                    name[ni++] = *p++;
                if (*p == '}') p++;
            } else if (isalnum((unsigned char)*p) || *p == '_' || *p == '?' ||
                       *p == '!' || *p == '#' || *p == '$') {
                if (*p == '?' || *p == '!' || *p == '#' || *p == '$')
                    name[ni++] = *p++;
                else
                    while (isalnum((unsigned char)*p) || *p == '_')
                        name[ni++] = *p++;
            } else {
                if (len + 2 >= cap) {
                    cap += 2;
                    out = realloc(out, cap);
                }
                out[len++] = '$';
                out[len] = '\0';
                continue;
            }
            name[ni] = '\0';
            char* val = expand_special(sh, name);
            size_t vlen = strlen(val);
            if (len + vlen + 1 >= cap) {
                cap = len + vlen + 2;
                out = realloc(out, cap);
            }
            memcpy(out + len, val, vlen);
            len += vlen;
            out[len] = '\0';
            free(val);
        } else if (*p == '\\' && !in_double_quotes) {
            p++;
            if (*p) {
                if (len + 2 >= cap) {
                    cap += 2;
                    out = realloc(out, cap);
                }
                out[len++] = *p++;
                out[len] = '\0';
            }
        } else {
            if (len + 2 >= cap) {
                cap += 2;
                out = realloc(out, cap);
            }
            out[len++] = *p++;
            out[len] = '\0';
        }
    }
    free(result);
    return out;
}

char** expand_argv(Shell* sh, char** argv, int argc, int* out_argc) {
    size_t cap = 16;
    char** out = malloc(cap * sizeof(char*));
    int n = 0;

    for (int i = 0; i < argc; i++) {
        char* exp = expand_word(sh, argv[i], 0);
        char* tok = strtok(exp, " \t");
        while (tok) {
            if ((size_t)n + 1 >= cap) {
                cap *= 2;
                out = realloc(out, cap * sizeof(char*));
            }
            out[n++] = strdup(tok);
            tok = strtok(NULL, " \t");
        }
        free(exp);
    }
    out[n] = NULL;
    *out_argc = n;
    return out;
}
