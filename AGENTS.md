# KinetiC — Agent Guide

> **MANDATORY**: Do not commit! The author reviews and commits all changes!

---

## What this project is

**KinetiC** is a high-performance, cloud-native **HTTP/1.1** server and edge proxy written in **C17**. It uses **libuv** for asynchronous I/O, deterministic RFC 9110/9112 state machines, bump arenas (`ktc_arena`), and non-owning wire octet slices (`ktc_str`).

The server is designed to evolve into a dynamic cloud-native reverse proxy (similar to Traefik, in pure C) that interacts with Docker Unix domain sockets (`/var/run/docker.sock`) and Kubernetes API streams.

---

## Non-negotiables

1. **Language & Standard**: Strict **C17** (`-std=c17`).
2. **API Prefix**: `ktc_` for all public functions, structs, enums, and types.
3. **Header Hierarchy**: All public headers reside in `include/ktc/` (`include/ktc/core/`, `include/ktc/http/`).
4. **HTTP Parsing**: Strict RFC 9110 / RFC 9112 state machines. No external parsing libraries (`llhttp`, `nghttp2`, etc.).
5. **Memory Safety**: No raw `malloc`/`free` thrashing in network loops. Use per-connection bump arenas (`ktc_arena`) and the global, non-owned, multiplexed slot pool.
6. **No Commits**: Agents must never run `git commit`.

---

## Repository Layout

```text
include/ktc/core/         Core headers: str.h, arena.h, config.h, connection.h
include/ktc/http/         HTTP headers: req_line.h, headers.h, body.h, response.h
src/core/                 Core implementations (str.c, arena.c, parseyml.c, connection.c)
src/http/                 HTTP parsing & state machines (req_line.c, headers.c, body.c, response.c)
src/main.c                Main entry point & libuv loop initialization
docs/STATE.md             Normative roadmap and RFC completion checklist
docs/DECISIONS.md         Architectural decision records (ADRs)
docs/invariants/          Normative RFC checklists (HTTP_1_1.md, CACHING.md)
tests/unit/               Unit test suites (basement/, http/)
tests/integration/        Integration test suites (test_connection_handling.py)
configs/                  YAML configuration files (example.yaml, test_config.yaml)
vcpkg.json                vcpkg package manifest
```

---

## C Best Practices & Design Patterns

All code contributions must adhere to the design patterns and best practices from **"Fluent C: Principles, Practices, and Patterns"** (Christopher Preschern) and **"Patterns in C"** (Adam Tornhill):

### 1. Lifetime & Ownership Management
* **Explicit Ownership**: Clear distinction between owning structures (`ktc_arena_t`, `ktc_conn_t`) and non-owning views (`ktc_str`).
* **Non-Owning Slices (`ktc_str`)**: Pass wire octets as `{const uint8_t *ptr, size_t len}` slices without duplicating memory or modifying raw buffers.
* **Bump Arena Allocations**: Allocate per-request objects in the connection's `ktc_arena_t`. Reset in $O(1)$ time via `ktc_arena_reset()` between persistent HTTP requests to eliminate heap fragmentation.
* **Reference-Counted Async Teardown**: Async resources managed by libuv handles use explicit reference counting (`pending_closes_cnt`) to guarantee that context structures are freed only after all `uv_close` callbacks execute.

### 2. Encapsulation & Header Boundaries
* **Opaque Types & Separation**: Keep private implementation details, helper functions, and internal state machine variables in `.c` files. Expose only stable, typed interfaces in `include/ktc/`.
* **Single Responsibility**: Maintain strong cohesion within translation units (`req_line.c` parses request lines; `headers.c` validates header fields; `body.c` resolves framing precedence).

### 3. State Machines & Stream Processing
* **Incremental FSMs**: Stream decoders (request-line, headers, chunked body) must process bytes incrementally across TCP chunk boundaries without assuming contiguous in-memory payloads.
* **Explicit Parser States**: State transitions must be driven by strict enums (`ktc_req_line_state_t`, `ktc_header_state_t`, `ktc_chunk_state_t`) and validate against invalid characters immediately.

### 4. Defensive Programming & Invariants
* **No Unsafe Standard Library Calls**: Never use `strcpy`, `strcat`, `sprintf`, or `atoi`. Use bounds-checked memory copies (`memcpy`), length-checked string utilities (`ktc_str_*`), and digit validation with overflow detection.
* **Return Values & Error Handling**: Functions return a `bool` status or specific error code enum. Output values are written through explicit output pointers (`out_*`).

---

## Build & Verify Commands

Always verify changes using the build and test targets:

```bash
# Run unit tests only
make test-u

# Run integration tests only
make test-i

# Run full CI validation (formatting check + clang-tidy lints + all tests)
make check
```
