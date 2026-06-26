# KinetiC — Agent Guide

Read this before making changes. This file is the primary context entry point for Cursor and other coding agents.

> MANDATORY: Do not commit! The author reviews and then commits always!

## What this project is

**KinetiC** is a high-performance, cross-platform HTTP server in C. It replaces an earlier effort ([cserve](https://github.com/Samandar-Komilov/cserve)) with a deliberate architecture: CMake, libuv, libconfig, master/worker IPC, and invariant-based HTTP state machines.

The author values **readable, explicit C** (*Fluent C* style): scoped resource cleanup, shallow call stacks, and state machines over ad-hoc flags.

## Non-negotiables

1. **Language**: C17. No C++.
2. **Build**: CMake 3.20+. Do not introduce Make-only or hand-rolled build steps.
3. **Event loop**: libuv only for async I/O (no raw epoll/kqueue/IOCP in application code).
4. **Config**: libconfig `.conf` files under `configs/`. No JSON/YAML for server config unless explicitly requested.
5. **HTTP**: Custom parsers inspired by llhttp—**state machines with documented invariants**. Do not pull in nghttp2, llhttp as a library, or similar without discussion.
6. **Process model** (when implemented): master supervises workers; workers are shared-nothing; IPC is performance-oriented.
7. **Scope discipline**: Minimal diffs. Match existing naming, layout, and error-handling style.

## Repository layout

```
include/kinetic/     Public API headers
src/                 Implementation + kinetic binary
tests/               CTest executables
cmake/               Dependencies.cmake, CompilerWarnings.cmake
configs/             Example and default .conf files
```

- Library code: `kinetic_core` static library (`config.c`, `version.c`, …).
- Binary: `kinetic` in `src/`.
- Link libuv only where I/O is needed (typically the binary and later server modules), not in pure parsing units.

## Build & verify

Always validate changes:

```bash
make check    # format-check + clang-tidy + tests
```

Or step by step:

```bash
make build
make format-check
make lint
make test
```

CMake equivalents still work (`cmake -B build …`). The root `Makefile` is a thin wrapper for configure/build/run/format/lint.

- `KINETIC_FETCH_DEPS=ON` (default): fetch libuv/libconfig if not on system.
- `KINETIC_BUILD_TESTS=OFF`: skip tests (avoid for normal development).
- Run `make format` before submitting C changes; config is `.clang-format` / `.clang-tidy`.

## Coding conventions

### Style

- Format with **clang-format** (`make format`); project config: `.clang-format` (LLVM-derived, 4 spaces, 100 columns).
- `snake_case` for functions and variables; `kinetic_` prefix for public API.
- Headers under `include/kinetic/` use include guards `KINETIC_*_H`.
- One responsibility per translation unit where practical.
- Prefer early returns over deep nesting.

### Resources (RAII)

Use explicit init/destroy pairs with a single exit path, or GCC/Clang `cleanup` attributes when portability is confirmed. Every `config_init`, `uv_*_init`, `malloc` must have a matching destroy/free on all paths.

```c
config_t cfg;
config_init(&cfg);
if (config_read_file(&cfg, path) != CONFIG_TRUE) {
    config_destroy(&cfg);
    return -1;
}
/* ... */
config_destroy(&cfg);
```

### Errors

- Library functions return `0` on success, `-1` (or typed error codes later) on failure.
- Log to `stderr` with a `kinetic:` prefix and actionable context.
- Do not abort in library code; reserve `assert` for programmer errors behind `NDEBUG` in release.

### HTTP (future)

When adding HTTP code:

- Define states as an enum; transitions in one place (table or switch with invariant comments).
- Document pre/post conditions per state in header comments.
- No recursive descent through callbacks; use a parse loop driven by available input bytes.
- Reference llhttp's semantics, do not copy its generated code wholesale.

### Process / IPC (future)

- Master: config reload, worker spawn/stop, signal handling.
- Worker: accept connections, run `uv_run`, no global mutable shared state.
- Prefer length-prefixed binary IPC over text protocols for hot paths.

## Dependencies

| Library | Role | CMake target (typical) |
|---------|------|-------------------------|
| libuv | Event loop, sockets, timers | `uv_a` or system `libuv` |
| libconfig | Parse `.conf` | `config` or `libconfig::libconfig` |

Resolve via `cmake/Dependencies.cmake`. Prefer linking imported targets over manual `include_directories`.

## Testing

- Unit tests: small `tests/test_*.c` files, plain `assert`, registered with `add_test` in `tests/CMakeLists.txt`.
- Smoke tests: run `kinetic` with `PASS_REGULAR_EXPRESSION` when appropriate.
- Add tests when behavior is non-trivial; skip tests for pure declarations or obvious constants.

## Roadmap awareness

Current phase: **0 — Bootstrap** (CMake, config load, libuv smoke loop).

Do not implement full HTTP serving, TLS, or master/worker spawn unless the user asks for that phase. When they do, implement incrementally and update this file's phase note.

## What not to do

- Do not commit `build/` or generated artifacts.
- Do not add Docker, CI, or large doc trees unless requested.
- Do not rewrite working code for style-only reasons.
- Do not introduce hidden macros or metaprogramming that obscures control flow.

## License

MIT. Preserve copyright headers on new files when the repo adds them.
