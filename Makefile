# KinetiC — developer convenience targets (CMake remains the build system).

SHELL := /bin/bash

BUILD_DIR   ?= build
BUILD_TYPE  ?= Debug
JOBS        ?= $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
CONFIG      ?= /etc/kinetic/kinetic.yaml
BINARY      := $(BUILD_DIR)/src/kinetic
CMAKE_FLAGS ?= -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Project sources only (never dependencies under build/ or vendored code).
SOURCES := $(shell find include src tests -type f \( -name '*.c' -o -name '*.h' \) ! -path 'src/external/*' ! -path 'include/ktc/external/*' 2>/dev/null | sort)
C_SOURCES := $(filter %.c,$(SOURCES))

CLANG_FORMAT ?= clang-format
CLANG_TIDY   ?= clang-tidy
CPPCHECK     ?= cppcheck

# compile_commands.json from CMake is authoritative for clang-tidy (-p build).

.PHONY: help all configure build rebuild run test clean distclean \
        format format-check lint lint-cppcheck check tools

help:
	@printf 'Usage: make <target>\n\nTargets:\n'
	@grep -E '^[a-zA-Z0-9_.-]+:.*##' $(MAKEFILE_LIST) | \
		awk 'BEGIN {FS = ":.*## "}; {printf "  %-16s %s\n", $$1, $$2}'

all: build

configure:
	cmake -B $(BUILD_DIR) $(CMAKE_FLAGS)

build: configure
	cmake --build $(BUILD_DIR) -j $(JOBS)

rebuild:
	$(MAKE) clean
	$(MAKE) build

build-release:
	$(MAKE) build BUILD_TYPE=Release

run: build
	./$(BINARY) $(CONFIG)

test: build ## Run all unit and integration tests
	ctest --test-dir $(BUILD_DIR) --output-on-failure

test-v: build ## Run all tests with full verbose assertion outputs
	ctest --test-dir $(BUILD_DIR) --verbose

test-u: build ## Run unit tests with detailed assertion outputs
	ctest --test-dir $(BUILD_DIR) -L unit --verbose

test-i: build ## Run integration tests with detailed assertion outputs
	ctest --test-dir $(BUILD_DIR) -L integration --verbose

format: tools
	@$(CLANG_FORMAT) -i $(SOURCES)
	@printf 'Formatted %s file(s).\n' "$(words $(SOURCES))"

format-check: tools
	@$(CLANG_FORMAT) --dry-run --Werror $(SOURCES)
	@printf 'Format check passed (%s file(s)).\n' "$(words $(SOURCES))"

lint: build tools
	@test -f $(BUILD_DIR)/compile_commands.json || { \
		printf 'Missing %s/compile_commands.json — run make build first.\n' "$(BUILD_DIR)"; \
		exit 1; \
	}
	@set -euo pipefail; \
	for src in $(C_SOURCES); do \
		abs="$(CURDIR)/$$src"; \
		printf 'clang-tidy: %s\n' "$$src"; \
		$(CLANG_TIDY) -p $(BUILD_DIR) "$$abs"; \
	done

lint-cppcheck: tools
	@command -v $(CPPCHECK) >/dev/null || { \
		printf 'cppcheck not found — install cppcheck or set CPPCHECK=…\n'; \
		exit 1; \
	}
	@$(CPPCHECK) --enable=warning,style,performance,portability \
		--error-exitcode=1 \
		--inline-suppr \
		--suppress=missingIncludeSystem \
		-I include \
		$(C_SOURCES)

check: format-check lint test

coverage: ## Generate code coverage reports using gcov
	cmake -B $(BUILD_DIR) $(CMAKE_FLAGS) -DKINETIC_COVERAGE=ON
	cmake --build $(BUILD_DIR) -j $(JOBS)
	ctest --test-dir $(BUILD_DIR) --output-on-failure
	@printf '\n--- Coverage Summary ---\n'
	@find $(BUILD_DIR)/src -name '*.gcda' | while read -r gcda; do \
		gcov -n -o "$$(dirname "$$gcda")" "$$gcda"; \
	done | grep -E "File|Lines executed"



clean:
	rm -rf $(BUILD_DIR)

distclean: clean
	rm -f compile_commands.json

tools:
	@command -v $(CLANG_FORMAT) >/dev/null || { printf 'clang-format not found\n'; exit 1; }
	@command -v $(CLANG_TIDY) >/dev/null || { printf 'clang-tidy not found\n'; exit 1; }
