# KinetiC — developer convenience targets (CMake remains the build system).

SHELL := /bin/bash

BUILD_DIR   ?= build
BUILD_TYPE  ?= Debug
JOBS        ?= $(shell nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
CONFIG      ?= configs/kinetic.yaml.example
BINARY      := $(BUILD_DIR)/src/kinetic
CMAKE_FLAGS ?= -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Project sources only (never dependencies under build/).
SOURCES := $(shell find include src tests -type f \( -name '*.c' -o -name '*.h' \) 2>/dev/null | sort)
C_SOURCES := $(filter %.c,$(SOURCES))

CLANG_FORMAT ?= clang-format
CLANG_TIDY   ?= clang-tidy
CPPCHECK     ?= cppcheck

# clang-tidy needs explicit includes when compile_commands.json uses GCC (/usr/bin/cc).
LINT_CFLAGS := -I$(CURDIR)/include -std=c17

.PHONY: help all configure build rebuild run test clean distclean \
        format format-check lint lint-cppcheck check tools

help: ## Show this help
	@printf 'Usage: make <target>\n\nTargets:\n'
	@grep -E '^[a-zA-Z0-9_.-]+:.*##' $(MAKEFILE_LIST) | \
		awk 'BEGIN {FS = ":.*## "}; {printf "  %-16s %s\n", $$1, $$2}'

all: build ## Alias for build

configure: ## Configure CMake (creates $(BUILD_DIR))
	cmake -B $(BUILD_DIR) $(CMAKE_FLAGS)

build: configure ## Build debug binary by default
	cmake --build $(BUILD_DIR) -j $(JOBS)

rebuild: ## Clean build tree and rebuild
	$(MAKE) clean
	$(MAKE) build

build-release: ## Configure and build with CMAKE_BUILD_TYPE=Release
	$(MAKE) build BUILD_TYPE=Release

run: build ## Run kinetic (CONFIG=path/to/kinetic.yaml, default: example config)
	./$(BINARY) $(CONFIG)

test: build ## Run CTest suite
	ctest --test-dir $(BUILD_DIR) --output-on-failure

format: tools ## Format project C sources with clang-format
	@$(CLANG_FORMAT) -i $(SOURCES)
	@printf 'Formatted %s file(s).\n' "$(words $(SOURCES))"

format-check: tools ## Fail if sources are not clang-format clean
	@$(CLANG_FORMAT) --dry-run --Werror $(SOURCES)
	@printf 'Format check passed (%s file(s)).\n' "$(words $(SOURCES))"

lint: build tools ## Run clang-tidy on project .c files
	@test -f $(BUILD_DIR)/compile_commands.json || { \
		printf 'Missing %s/compile_commands.json — run make build first.\n' "$(BUILD_DIR)"; \
		exit 1; \
	}
	@set -euo pipefail; \
	for src in $(C_SOURCES); do \
		abs="$(CURDIR)/$$src"; \
		printf 'clang-tidy: %s\n' "$$src"; \
		$(CLANG_TIDY) -p $(BUILD_DIR) "$$abs" -- $(LINT_CFLAGS); \
	done

lint-cppcheck: tools ## Run cppcheck on project sources (optional tool)
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

check: format-check lint test ## format-check + clang-tidy + tests

clean: ## Remove build directory
	rm -rf $(BUILD_DIR)

distclean: clean ## Remove build dir and editor artifacts
	rm -f compile_commands.json

tools: ## Verify developer tools are available
	@command -v $(CLANG_FORMAT) >/dev/null || { printf 'clang-format not found\n'; exit 1; }
	@command -v $(CLANG_TIDY) >/dev/null || { printf 'clang-tidy not found\n'; exit 1; }
