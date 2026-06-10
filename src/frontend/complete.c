#include <lsh/complete.h>
#include <lsh/builtin.h>
#include <sys/stat.h>

static const char* extra_commands[] = {
    "jobs", "fg", "bg", NULL};

static int has_prefix(const char* prefix, const char* name) {
    if (!prefix[0]) return 1;
    return strncmp(prefix, name, strlen(prefix)) == 0;
}

static void list_add(CompletionList* list, const char* item) {
    for (int i = 0; i < list->count; i++) {
        if (strcmp(list->items[i], item) == 0)
            return;
    }
    list->items = realloc(list->items, (size_t)(list->count + 1) * sizeof(char*));
    list->items[list->count++] = strdup(item);
}

void completion_free(CompletionList* list) {
    if (!list) return;
    for (int i = 0; i < list->count; i++)
        free(list->items[i]);
    free(list->items);
    list->items = NULL;
    list->count = 0;
}

static int is_command_position(const char* line, int word_start) {
    int i = word_start - 1;
    while (i >= 0 && (line[i] == ' ' || line[i] == '\t')) i--;
    if (i < 0) return 1;
    if (line[i] == '|' || line[i] == ';') return 1;
    if (i > 0 && line[i] == '&' && line[i - 1] == '&') return 1;
    if (i > 0 && line[i] == '|' && line[i - 1] == '|') return 1;
    return 0;
}

static int path_is_executable(const char* path) {
    struct stat st;
    if (stat(path, &st) < 0) return 0;
    return S_ISREG(st.st_mode) && (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH));
}

static void gather_commands(const char* prefix, CompletionList* out) {
    for (int i = 0; i < builtin_count(); i++) {
        const char* name = builtin_name(i);
        if (name && has_prefix(prefix, name))
            list_add(out, name);
    }
    for (int i = 0; extra_commands[i]; i++) {
        if (has_prefix(prefix, extra_commands[i]))
            list_add(out, extra_commands[i]);
    }

    const char* path_env = getenv("PATH");
    if (!path_env) return;

    char path_copy[4096];
    snprintf(path_copy, sizeof(path_copy), "%s", path_env);

    char* dir = strtok(path_copy, ":");
    while (dir) {
        DIR* d = opendir(dir);
        if (d) {
            struct dirent* ent;
            while ((ent = readdir(d))) {
                if (ent->d_name[0] == '.') continue;
                if (!has_prefix(prefix, ent->d_name)) continue;
                char full[PATH_MAX];
                snprintf(full, sizeof(full), "%s/%s", dir, ent->d_name);
                if (path_is_executable(full))
                    list_add(out, ent->d_name);
            }
            closedir(d);
        }
        dir = strtok(NULL, ":");
    }
}

static void expand_tilde(const char* word, char* out, size_t outsz) {
    if (word[0] != '~') {
        snprintf(out, outsz, "%s", word);
        return;
    }
    const char* home = getenv("HOME");
    if (!home) {
        struct passwd* pw = getpwuid(getuid());
        home = pw ? pw->pw_dir : "";
    }
    if (word[1] == '\0' || word[1] == '/') {
        snprintf(out, outsz, "%s%s", home ? home : "", word + 1);
    } else {
        snprintf(out, outsz, "%s", word);
    }
}

static void gather_paths(const char* word, CompletionList* out) {
    char expanded[PATH_MAX];
    expand_tilde(word, expanded, sizeof(expanded));

    char dir[PATH_MAX];
    char base[PATH_MAX];
    const char* slash = strrchr(expanded, '/');
    if (!slash) {
        strcpy(dir, ".");
        snprintf(base, sizeof(base), "%s", expanded);
    } else {
        size_t dlen = (size_t)(slash - expanded);
        if (dlen == 0) {
            strcpy(dir, "/");
        } else {
            if (dlen >= sizeof(dir)) dlen = sizeof(dir) - 1;
            memcpy(dir, expanded, dlen);
            dir[dlen] = '\0';
        }
        snprintf(base, sizeof(base), "%s", slash + 1);
    }

    int dir_trailing = slash && slash[1] == '\0';
    if (dir_trailing) {
        base[0] = '\0';
    }

    DIR* d = opendir(dir[0] ? dir : ".");
    if (!d) return;

    size_t word_dir_len = slash ? (size_t)(slash - word + 1) : 0;
    struct dirent* ent;
    while ((ent = readdir(d))) {
        if (ent->d_name[0] == '.' && base[0] != '.') continue;
        if (!has_prefix(base, ent->d_name)) continue;

        char candidate[PATH_MAX];
        if (word_dir_len > 0) {
            snprintf(candidate, sizeof(candidate), "%.*s%s",
                     (int)word_dir_len, word, ent->d_name);
        } else {
            snprintf(candidate, sizeof(candidate), "%s", ent->d_name);
        }

        char full[PATH_MAX];
        if (strcmp(dir, ".") == 0)
            snprintf(full, sizeof(full), "%s", ent->d_name);
        else
            snprintf(full, sizeof(full), "%s/%s", dir, ent->d_name);

        struct stat st;
        if (stat(full, &st) == 0 && S_ISDIR(st.st_mode)) {
            size_t clen = strlen(candidate);
            if (clen + 1 < sizeof(candidate)) {
                candidate[clen] = '/';
                candidate[clen + 1] = '\0';
            }
        }
        list_add(out, candidate);
    }
    closedir(d);
}

int completion_gather(Shell* sh, const char* line, int word_start, int word_end,
                      CompletionList* out) {
    (void)sh;
    out->items = NULL;
    out->count = 0;

    char word[LSH_MAX_LINE];
    int wlen = word_end - word_start;
    if (wlen <= 0 || wlen >= LSH_MAX_LINE) return 0;
    memcpy(word, line + word_start, (size_t)wlen);
    word[wlen] = '\0';

    if (is_command_position(line, word_start))
        gather_commands(word, out);
    else
        gather_paths(word, out);

    return out->count;
}

char* completion_common_prefix(CompletionList* list) {
    if (list->count == 0) return strdup("");
    size_t len = strlen(list->items[0]);
    for (int i = 1; i < list->count; i++) {
        size_t j = 0;
        while (j < len && list->items[i][j] && list->items[0][j] == list->items[i][j])
            j++;
        len = j;
    }
    char* out = malloc(len + 1);
    memcpy(out, list->items[0], len);
    out[len] = '\0';
    return out;
}
