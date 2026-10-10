# Builds the tcc-js kit with TinyCC. See README.md.
TCC ?= tcc
CC ?= cc
CFLAGS = -std=c99 -Wall -Werror -Isrc
CLANG_FLAGS = -std=c99 -Wall -Wextra -Wswitch-enum -Werror -fsyntax-only -Isrc
FRONT = src/front/base.c src/front/lexer.c src/front/parser.c src/front/front.c src/front/eval.c src/front/check.c
TARGET = src/json.c src/js.c
SRC = src/main.c $(FRONT) $(TARGET)
HEADERS = $(wildcard src/*.h src/front/*.h)
RG = rg --no-config --color never
DOC_FILES = --hidden --no-ignore -g '*.md' -g '!.git' -g '!build' -g '!node_modules' -g '!scratch'
DOC_SKIP = -g '!design/DESIGN.md'
PLACEHOLDER = \{\{|\}\}|\{\s*(?:LANG|HOST)\s*\}

.PHONY: build check check-clang clean doc-check

build: build/langc

build/domain.c: domain/domain.lang gen/embed.c
	mkdir -p build
	$(TCC) -run gen/embed.c domain/domain.lang build/domain.c

build/langc: $(SRC) $(HEADERS) build/domain.c
	$(TCC) $(CFLAGS) -o build/langc $(SRC) build/domain.c

build/backend-regressions: test/backend-regressions.c $(SRC) $(HEADERS) build/domain.c
	$(TCC) $(CFLAGS) -o $@ test/backend-regressions.c $(FRONT) src/js.c build/domain.c

check-clang: build/domain.c
	$(CC) $(CLANG_FLAGS) $(SRC) build/domain.c
	$(CC) $(CLANG_FLAGS) test/backend-regressions.c

check: build/langc build/backend-regressions check-clang
	build/backend-regressions
	sh test/gate.sh
	TCC="$(TCC)" node test/regressions.js
	node test/printer-regressions.js
	node test/app-regressions.js

# Fail on an em-dash or en-dash in a Markdown file, or on a malformed
# placeholder (the checks of lang-template bin/doc-check.pl). Skip
# design/DESIGN.md: it is verbatim and can hold a dash.
doc-check:
	@set -eu; \
	  scan() { status=0; "$$@" || status=$$?; [ "$$status" -le 1 ] || return "$$status"; }; \
	  dashes=$$(scan $(RG) -H -n --no-heading --sort path $(DOC_FILES) $(DOC_SKIP) '\x{2013}|\x{2014}' .); \
	  dashes=$$(printf '%s\n' "$$dashes" | scan $(RG) -N -o -r '$$1: em-dash or en-dash' '^(?:\./)?(.*:[0-9]+):'); \
	  placeholders=$$(scan $(RG) -H -n --no-heading --sort path $(DOC_FILES) $(DOC_SKIP) '$(PLACEHOLDER)' .); \
	  placeholders=$$(printf '%s\n' "$$placeholders" | scan $(RG) -N --passthru -r '' '\{\{(?:LANG|HOST)\}\}'); \
	  placeholders=$$(printf '%s\n' "$$placeholders" | scan $(RG) -N -o -r '$$1: malformed placeholder' '^(?:\./)?(.*:[0-9]+):.*(?:$(PLACEHOLDER))'); \
	  { [ -z "$$dashes" ] || printf '%s\n' "$$dashes"; \
	    [ -z "$$placeholders" ] || printf '%s\n' "$$placeholders"; } \
	  | awk '{ print } END { printf "doc-check: %d problem(s)\n", NR; exit (NR > 0) }'

clean:
	rm -rf build
