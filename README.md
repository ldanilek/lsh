# lsh — a minimal Bash-like shell

`lsh` is a compact POSIX shell written in pure C with no external dependencies beyond the standard C library and POSIX APIs available on typical Linux/macOS/gcc environments (including Coderpad).

## Build

```bash
make
```

## Run

```bash
./lsh              # interactive mode
./lsh script.sh    # run a script file
./lsh -c 'echo hi' # run a single command
```

## Test

```bash
make test
```

## Features

### Implemented

- **Command execution** — external programs via `fork`/`execvp`
- **Builtins** — `cd`, `pwd`, `echo`, `exit`, `export`, `unset`, `true`, `false`, `:`, `type`, `kill`, `history`, `source`/`.`, `jobs`, `fg`, `bg`
- **Pipelines** — `cmd1 | cmd2 | ...`
- **Redirections** — `<`, `>`, `>>`, `2>`, `2>>`, `&>`
- **Variables** — `NAME=value` prefixes, `$VAR`, `${VAR}`, `export`, `unset`
- **Special parameters** — `$?` (last exit status), `$$`/`$PPID`, `$!` (last background PID)
- **Logical operators** — `&&`, `||`, `;`
- **Background jobs** — `cmd &`, job table, `jobs`/`fg`/`bg`
- **Quoting** — single quotes, double quotes, backslash escapes
- **Comments** — `# ...`
- **Globbing** — `*` and `?` in unquoted words
- **Tilde expansion** — `~` and `~/path`
- **History** — up/down arrow recall in interactive mode
- **Full-screen programs** — `less`, `vim`, and similar TUI apps receive the controlling terminal
- **Emacs line editing** (interactive):
  - `Ctrl-A` / `Ctrl-E` — beginning/end of line
  - `Ctrl-B` / `Ctrl-F` — backward/forward character
  - `Option-Left` / `Option-Right` (or `Alt-b` / `Alt-f`) — backward/forward word
  - `Ctrl-P` / `Ctrl-N` — previous/next history (also arrow keys)
  - `Ctrl-K` — kill to end of line
  - `Ctrl-U` — kill whole line
  - `Ctrl-W` — kill word backward
  - `Backspace` / `Ctrl-D` — delete char / EOF
  - `Tab` — complete commands (first word or after `|`) or file paths
- **Signal handling** — Ctrl-C forwards to foreground process group
- **Scripts** — `lsh file.sh` and `source file`

## Unimplemented features

These are intentionally omitted or only partially supported:

- **Full Bash compatibility** — syntax and semantics differ in many edge cases
- **Subshells** — `( commands )`
- **Command substitution** — `$(...)` and backticks
- **Arithmetic expansion** — `$(( ... ))`
- **Brace expansion** — `{a,b,c}`
- **Process substitution** — `<(...)` and `>(...)`
- **Here documents** — `<<EOF`
- **Aliases** — `alias`/`unalias`
- **Programmable completion** — `complete` builtin, context-specific completion
- **Advanced globbing** — `[...]` character classes, `**`, extended globs
- **Job control signals** — full `Ctrl-Z` / tty stop-start parity with Bash
- **Coprocesses**, **select**, **case** statements, **for/while/until** loops
- **Functions** — `name() { ... }`
- **Local variables**, **readonly**, **declare**, **set -o**
- **Trap** builtin and user-defined signal handlers
- **Path hashing**, **command -v** caching
- **Locale/multibyte** — ASCII-oriented input editing
- **Restricted mode**, **shopt**, **getopts**
- **Word splitting** on `$IFS` for all contexts (only basic splitting)
- **`eval`**, **`exec`**, **`wait`**, **`times`**, **`ulimit`**
- **Regex matching** — `=~`
- **Array variables**
- **Prompt escape sequences** — `\w`, `\h`, etc. in `PS1` (literal prompt string only)

Contributions welcome for any of the above.

## Project layout

```
include/lsh/          Public headers (installed-style API)
  shell.h             Core types and shell state
  ast.h lexer.h parser.h
  execute.h expand.h jobs.h
  builtin.h vars.h signals.h
  input.h complete.h

src/
  core/               Process lifecycle and signals
    main.c            Entry point, REPL, script driver
    shell.c           Shell init/teardown
    signals.c         Terminal and signal handling
  parse/              Lexing and parsing
    lexer.c parser.c ast.c
  runtime/            Execution engine
    execute.c         Pipelines, redirects, fork/exec
    expand.c          Variables, globs, tilde
    jobs.c            Background job management
  builtins/           Shell builtin commands
    builtin.c
  env/                Variable and environment table
    vars.c
  frontend/           Interactive UI
    input.c           Line editor and history
    complete.c        Tab completion

build/obj/            Compiled object files (mirrors src/)
tests/
  run_tests.sh        Integration test suite
```

## License

MIT
