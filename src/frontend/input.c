#include <lsh/input.h>
#include <lsh/complete.h>
#include <lsh/signals.h>

static void enable_raw_mode(Shell* sh) {
    if (!sh->interactive || !sh->shell_termios_saved) return;
    struct termios raw = sh->shell_termios;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

static void disable_raw_mode(Shell* sh) {
    if (!sh->interactive || !sh->shell_termios_saved) return;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &sh->shell_termios);
}

void history_add(Shell* sh, const char* line) {
    if (!line || !line[0]) return;
    if (sh->history_count > 0 &&
        strcmp(sh->history[sh->history_count - 1], line) == 0)
        return;
    if (sh->history_count >= LSH_MAX_HISTORY) {
        free(sh->history[0]);
        memmove(&sh->history[0], &sh->history[1],
                (size_t)(LSH_MAX_HISTORY - 1) * sizeof(char*));
        sh->history_count = LSH_MAX_HISTORY - 1;
    }
    sh->history[sh->history_count++] = strdup(line);
}

static void buf_insert(char* buf, int* len, int* pos, char c) {
    if (*len >= LSH_MAX_LINE - 1) return;
    memmove(buf + *pos + 1, buf + *pos, (size_t)(*len - *pos));
    buf[*pos] = c;
    (*len)++;
    (*pos)++;
    buf[*len] = '\0';
}

static void buf_delete_forward(char* buf, int* len, int* pos) {
    if (*pos >= *len) return;
    memmove(buf + *pos, buf + *pos + 1, (size_t)(*len - *pos));
    (*len)--;
    buf[*len] = '\0';
}

static void buf_delete_backward(char* buf, int* len, int* pos) {
    if (*pos <= 0) return;
    (*pos)--;
    buf_delete_forward(buf, len, pos);
}

static void buf_kill_line(char* buf, int* len, int* pos) {
    buf[*pos] = '\0';
    *len = *pos;
}

static void buf_kill_word(char* buf, int* len, int* pos) {
    while (*pos > 0 && buf[*pos - 1] == ' ')
        buf_delete_backward(buf, len, pos);
    while (*pos > 0 && buf[*pos - 1] != ' ')
        buf_delete_backward(buf, len, pos);
}

static int is_word_char(char c) {
    return c != ' ' && c != '\t';
}

static void move_word_back(const char* buf, int* pos) {
    if (*pos == 0) return;
    (*pos)--;
    while (*pos > 0 && !is_word_char(buf[*pos - 1])) (*pos)--;
    while (*pos > 0 && is_word_char(buf[*pos - 1])) (*pos)--;
}

static void move_word_forward(const char* buf, int len, int* pos) {
    if (*pos >= len) return;
    while (*pos < len && !is_word_char(buf[*pos])) (*pos)++;
    while (*pos < len && is_word_char(buf[*pos])) (*pos)++;
}

typedef enum {
    ESC_NONE,
    ESC_HISTORY_UP,
    ESC_HISTORY_DOWN,
    ESC_CHAR_LEFT,
    ESC_CHAR_RIGHT,
    ESC_WORD_LEFT,
    ESC_WORD_RIGHT,
    ESC_LINE_START,
    ESC_LINE_END,
} EscAction;

static int csi_has_modifier(const char* csi, int mod) {
    char needle[8];
    snprintf(needle, sizeof(needle), ";%d", mod);
    return strstr(csi, needle) != NULL;
}

static int csi_is_word_motion(const char* csi) {
    if (csi_has_modifier(csi, 3) || csi_has_modifier(csi, 5)) return 1;
    return csi[0] == '5' && (csi[1] == 'D' || csi[1] == 'C');
}

static EscAction parse_escape_sequence(void) {
    char lead;
    if (read(STDIN_FILENO, &lead, 1) != 1) return ESC_NONE;

    if (lead == 'b') return ESC_WORD_LEFT;
    if (lead == 'f') return ESC_WORD_RIGHT;

    if (lead == 'O') {
        char final;
        if (read(STDIN_FILENO, &final, 1) != 1) return ESC_NONE;
        if (final == 'D') return ESC_CHAR_LEFT;
        if (final == 'C') return ESC_CHAR_RIGHT;
        if (final == 'H') return ESC_LINE_START;
        if (final == 'F') return ESC_LINE_END;
        return ESC_NONE;
    }

    if (lead != '[') return ESC_NONE;

    char csi[32];
    int n = 0;
    while (n < (int)sizeof(csi) - 1) {
        if (read(STDIN_FILENO, &csi[n], 1) != 1) return ESC_NONE;
        if (csi[n] >= '@' && csi[n] <= '~') {
            n++;
            break;
        }
        n++;
    }
    csi[n] = '\0';
    if (n == 0) return ESC_NONE;

    char final = csi[n - 1];
    int word = csi_is_word_motion(csi);

    if (final == 'A') return ESC_HISTORY_UP;
    if (final == 'B') return ESC_HISTORY_DOWN;
    if (final == 'C') return word ? ESC_WORD_RIGHT : ESC_CHAR_RIGHT;
    if (final == 'D') return word ? ESC_WORD_LEFT : ESC_CHAR_LEFT;
    if (final == 'H') return ESC_LINE_START;
    if (final == 'F') return ESC_LINE_END;

    return ESC_NONE;
}

static void word_at_cursor(const char* buf, int len, int pos, int* ws, int* we) {
    *ws = pos;
    while (*ws > 0 && buf[*ws - 1] != ' ' && buf[*ws - 1] != '\t') (*ws)--;
    *we = pos;
    while (*we < len && buf[*we] != ' ' && buf[*we] != '\t') (*we)++;
}

static void buf_replace_word(char* buf, int* len, int* pos, int ws, int we,
                             const char* replacement) {
    size_t rlen = strlen(replacement);
    size_t tail = (size_t)(*len - we);
    size_t new_len = (size_t)ws + rlen + tail;
    if (new_len >= LSH_MAX_LINE) return;

    memmove(buf + ws + rlen, buf + we, tail + 1);
    memcpy(buf + ws, replacement, rlen);
    *len = (int)new_len;
    *pos = ws + (int)rlen;
}

static void refresh_line(const char* prompt, char* buf, int len, int pos) {
    char seq[64];
    snprintf(seq, sizeof(seq), "\r%s%s", prompt, buf);
    write(STDOUT_FILENO, seq, strlen(seq));
    snprintf(seq, sizeof(seq), "\033[K");
    write(STDOUT_FILENO, seq, strlen(seq));
    if (pos < len) {
        snprintf(seq, sizeof(seq), "\033[%dD", len - pos);
        write(STDOUT_FILENO, seq, strlen(seq));
    }
}

static char* read_line_interactive(Shell* sh, const char* prompt) {
    char buf[LSH_MAX_LINE];
    int len = 0, pos = 0;
    buf[0] = '\0';
    int hist_idx = sh->history_count;

    enable_raw_mode(sh);
    write(STDOUT_FILENO, prompt, strlen(prompt));

    while (1) {
        char c;
        if (read(STDIN_FILENO, &c, 1) != 1) {
            disable_raw_mode(sh);
            return NULL;
        }

        if (c == '\n' || c == '\r') {
            write(STDOUT_FILENO, "\n", 1);
            disable_raw_mode(sh);
            return strdup(buf);
        }

        if (c == 127 || c == 8) { /* backspace */
            buf_delete_backward(buf, &len, &pos);
            refresh_line(prompt, buf, len, pos);
            continue;
        }

        if (c == 4) { /* Ctrl-D */
            if (len == 0) {
                write(STDOUT_FILENO, "\n", 1);
                disable_raw_mode(sh);
                return NULL;
            }
            buf_delete_forward(buf, &len, &pos);
            refresh_line(prompt, buf, len, pos);
            continue;
        }

        if (c == 1) { /* Ctrl-A */
            pos = 0;
            refresh_line(prompt, buf, len, pos);
            continue;
        }
        if (c == 5) { /* Ctrl-E */
            pos = len;
            refresh_line(prompt, buf, len, pos);
            continue;
        }
        if (c == 2) { /* Ctrl-B */
            if (pos > 0) pos--;
            refresh_line(prompt, buf, len, pos);
            continue;
        }
        if (c == 6) { /* Ctrl-F */
            if (pos < len) pos++;
            refresh_line(prompt, buf, len, pos);
            continue;
        }
        if (c == 11) { /* Ctrl-K */
            buf_kill_line(buf, &len, &pos);
            refresh_line(prompt, buf, len, pos);
            continue;
        }
        if (c == 21) { /* Ctrl-U */
            pos = 0;
            buf_kill_line(buf, &len, &pos);
            refresh_line(prompt, buf, len, pos);
            continue;
        }
        if (c == 23) { /* Ctrl-W */
            buf_kill_word(buf, &len, &pos);
            refresh_line(prompt, buf, len, pos);
            continue;
        }

        if (c == '\t') {
            int ws, we;
            word_at_cursor(buf, len, pos, &ws, &we);
            CompletionList matches;
            completion_gather(sh, buf, ws, we, &matches);

            if (matches.count == 0) {
                write(STDOUT_FILENO, "\a", 1);
            } else if (matches.count == 1) {
                buf_replace_word(buf, &len, &pos, ws, we, matches.items[0]);
                refresh_line(prompt, buf, len, pos);
            } else {
                char* common = completion_common_prefix(&matches);
                int cur_len = we - ws;
                if ((int)strlen(common) > cur_len) {
                    buf_replace_word(buf, &len, &pos, ws, we, common);
                    refresh_line(prompt, buf, len, pos);
                } else {
                    write(STDOUT_FILENO, "\n", 1);
                    for (int i = 0; i < matches.count; i++) {
                        write(STDOUT_FILENO, matches.items[i],
                              strlen(matches.items[i]));
                        write(STDOUT_FILENO, "  ", 2);
                    }
                    write(STDOUT_FILENO, "\n", 1);
                    refresh_line(prompt, buf, len, pos);
                }
                free(common);
            }
            completion_free(&matches);
            continue;
        }

        if (c == 27) {
            EscAction action = parse_escape_sequence();
            if (action == ESC_HISTORY_UP) {
                if (hist_idx > 0) {
                    hist_idx--;
                    strcpy(buf, sh->history[hist_idx]);
                    len = (int)strlen(buf);
                    pos = len;
                    refresh_line(prompt, buf, len, pos);
                }
            } else if (action == ESC_HISTORY_DOWN) {
                if (hist_idx < sh->history_count) {
                    hist_idx++;
                    if (hist_idx == sh->history_count) {
                        buf[0] = '\0';
                        len = 0;
                        pos = 0;
                    } else {
                        strcpy(buf, sh->history[hist_idx]);
                        len = (int)strlen(buf);
                        pos = len;
                    }
                    refresh_line(prompt, buf, len, pos);
                }
            } else if (action == ESC_CHAR_LEFT) {
                if (pos > 0) pos--;
                refresh_line(prompt, buf, len, pos);
            } else if (action == ESC_CHAR_RIGHT) {
                if (pos < len) pos++;
                refresh_line(prompt, buf, len, pos);
            } else if (action == ESC_WORD_LEFT) {
                move_word_back(buf, &pos);
                refresh_line(prompt, buf, len, pos);
            } else if (action == ESC_WORD_RIGHT) {
                move_word_forward(buf, len, &pos);
                refresh_line(prompt, buf, len, pos);
            } else if (action == ESC_LINE_START) {
                pos = 0;
                refresh_line(prompt, buf, len, pos);
            } else if (action == ESC_LINE_END) {
                pos = len;
                refresh_line(prompt, buf, len, pos);
            }
            continue;
        }

        if (c >= 32 && c <= 126) {
            buf_insert(buf, &len, &pos, c);
            refresh_line(prompt, buf, len, pos);
        }
    }
}

char* input_read_line(Shell* sh, const char* prompt) {
    if (sh->interactive && isatty(STDIN_FILENO))
        return read_line_interactive(sh, prompt);

    char* line = NULL;
    size_t cap = 0;
    ssize_t n = getline(&line, &cap, stdin);
    if (n < 0) {
        free(line);
        return NULL;
    }
    while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
        line[--n] = '\0';
    return line;
}
