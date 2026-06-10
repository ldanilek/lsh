CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -Werror -pedantic -O2 -Iinclude
LDFLAGS =

BUILD_DIR = build/obj

SRCS = src/core/main.c \
       src/core/shell.c \
       src/core/signals.c \
       src/parse/ast.c \
       src/parse/lexer.c \
       src/parse/parser.c \
       src/runtime/execute.c \
       src/runtime/expand.c \
       src/runtime/jobs.c \
       src/builtins/builtin.c \
       src/env/vars.c \
       src/frontend/input.c \
       src/frontend/complete.c

OBJS = $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SRCS))

CLANG_FORMAT ?= clang-format
FMT_FILES := $(shell find include src -type f \( -name '*.c' -o -name '*.h' \))

.PHONY: all clean test lint

all: lsh

lsh: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS)

$(BUILD_DIR)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

test: lsh
	./tests/run_tests.sh

lint:
	@test -n "$(FMT_FILES)" || (echo "no C sources found" && exit 1)
	$(CLANG_FORMAT) --dry-run --Werror $(FMT_FILES)

clean:
	rm -rf lsh build
