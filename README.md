# KinetiC

A high-performance, cross-platform HTTP server written in C. KinetiC is a ground-up rewrite informed by lessons from [cserve](https://github.com/Samandar-Komilov/cserve): explicit architecture, formal HTTP state machines, and code you can read without fighting the runtime.

## Goals

- **C all the way down** — no hidden magic; every layer is inspectable.
- **Cross-platform I/O** — [libuv](https://libuv.org) for the event loop (Linux, macOS, Windows).
- **Master/worker model** — one supervisor process, many worker processes/threads, IPC tuned for throughput.
- **Invariant-based HTTP** — parsers and protocol handling as explicit state machines, inspired by [llhttp](https://github.com/nodejs/llhttp), implemented incrementally with a clear roadmap.
- **`.conf` configuration** — [libconfig](https://hyperrealm.github.io/libconfig/) for structured, human-readable settings.
- **Readable C** — patterns from *Fluent C* (RAII-style resource scope, flat control flow, no callback hell).

## Status

Early bootstrap. The repository currently provides:

- CMake build with libuv and libconfig (system packages or automatic FetchContent fallback)
- A hello-world binary that loads config, runs a libuv loop, and prints startup info
- CTest unit and smoke tests

HTTP serving, process model, and IPC are planned; see [Roadmap](#roadmap).

## Requirements

- CMake 3.20+
- C17 compiler (GCC, Clang, or MSVC)
- Git (when FetchContent downloads dependencies)

Optional system packages (used when present; otherwise fetched at configure time):

- `libuv` (e.g. `libuv-devel` on Fedora, `libuv1-dev` on Debian)
- `libconfig` (e.g. `libconfig-devel`, `libconfig-dev`)

Developer tooling (formatting and lint):

- `clang` / `clang-tools-extra` — `clang-format`, `clang-tidy` (Fedora: `clang-tools-extra`)
- `cppcheck` — optional static analysis (`make lint-cppcheck`)

## Build

With **Make** (recommended for day-to-day work):

```bash
make build          # Debug (default)
make build-release  # Release
make run            # build + run with example config
make test           # build + CTest
make help           # all targets
```

Equivalent **CMake** commands:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Debug build with `compile_commands.json` for clang-tidy:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```

Disable automatic dependency fetching (fail if system libs are missing):

```bash
cmake -B build -DKINETIC_FETCH_DEPS=OFF
```

## Run

```bash
make run
# or with a specific config:
make run CONFIG=configs/kinetic.conf.example
```

Copy the example config if you want a local default:

```bash
cp configs/kinetic.conf.example configs/kinetic.conf
make run CONFIG=configs/kinetic.conf
```

Example output:

```
Hello from kinetic 0.1.0 (libuv 1.x.x)
Configured to listen on port 8080
```

## Test

```bash
make test
```

## Format & lint

```bash
make format        # apply clang-format (.clang-format)
make format-check  # CI-style check (no writes)
make lint          # clang-tidy (needs make build first)
make lint-cppcheck # optional cppcheck pass
make check         # format-check + lint + test
```

Config files: `.clang-format`, `.clang-tidy` at the repository root.

## Project layout

```
kinetic/
├── AGENTS.md              # AI / agent context (Cursor entry point)
├── cmake/                 # CMake modules (dependencies, warnings)
├── configs/               # Example .conf files
├── include/kinetic/       # Public headers
├── src/                   # Core library and main binary
└── tests/                 # CTest targets
```

## Roadmap

| Phase | Focus |
|-------|--------|
| **0 — Bootstrap** | CMake, libuv loop, libconfig, tests *(current)* |
| **1 — Process model** | Master/worker spawn, shared-nothing workers, graceful reload |
| **2 — HTTP core** | Incremental request-line / header / body state machines |
| **3 — Server** | Listen sockets, connection lifecycle, minimal static responses |
| **4 — IPC & tuning** | Accept distribution, stats, backpressure |
| **5 — Production** | TLS termination hooks, observability, hardening |

## Design notes

### HTTP as state machines

Request parsing is modeled as explicit states and transitions with documented invariants (e.g. "no body until `Content-Length` or chunked framing is known"). Each state has a single responsibility; invalid transitions are rejected at compile time where possible and asserted in debug builds.

### Master / workers

The master owns configuration and worker lifecycle. Workers run the libuv event loop and own connections. IPC carries accept handoff, health, and reload signals—details land in Phase 1.

### Configuration

Settings live in libconfig `.conf` files. The schema will grow with the server; see `configs/kinetic.conf.example` for the initial `kinetic { ... }` group.

## License

MIT — see [LICENSE](LICENSE).

## Author

Samandar Komilov — previously [cserve](https://github.com/Samandar-Komilov/cserve); this is the clean-slate successor.
