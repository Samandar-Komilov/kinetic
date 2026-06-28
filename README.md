<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)"
            srcset="data/logo/kinetic-logo-dark.png">
    <img src="data/logo/kinetic-logo-light.png" height="72" alt="KinetiC">
  </picture>
</p>

<h1 align="center">KinetiC</h1>

<p align="center">
  High-performance, cross-platform <strong>HTTP/1.1</strong> server in C — built incrementally, RFC by RFC, with you writing every layer.
</p>

<p align="center">
  <a href="https://github.com/Samandar-Komilov/kinetic/blob/master/LICENSE"><img src="https://img.shields.io/badge/license-MIT-blue.svg" alt="License: MIT"></a>
  <a href="https://github.com/Samandar-Komilov/kinetic"><img src="https://img.shields.io/badge/version-0.1.0-7c3aed" alt="Version 0.1.0"></a>
  <img src="https://img.shields.io/badge/status-early%20development-yellow" alt="Early development">
  <img src="https://img.shields.io/badge/language-C-orange.svg" alt="Language: C">
  <img src="https://img.shields.io/badge/standard-C17-00599C?logo=c&logoColor=white" alt="C17">
  <img src="https://img.shields.io/badge/HTTP-1.1-007EC6" alt="HTTP/1.1">
  <img src="https://img.shields.io/badge/RFC-9110%20%7C%209112-555" alt="RFC 9110 / 9112">
  <img src="https://img.shields.io/badge/I%2FO-libuv-403C3D?logo=libuv&logoColor=white" alt="libuv">
  <img src="https://img.shields.io/badge/config-YAML-cb171e?logo=yaml&logoColor=white" alt="YAML config">
  <img src="https://img.shields.io/badge/build-CMake-064F8C?logo=cmake&logoColor=white" alt="CMake">
  <img src="https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey" alt="Cross-platform">
</p>

<p align="center">
  <a href="docs/notebooks/README.md">Notebooks</a>
  ·
  <a href="docs/http1.1/INVARIANTS.md">HTTP invariants</a>
  ·
  <a href="configs/kinetic.yaml.example">Example config</a>
  ·
  <a href="https://github.com/Samandar-Komilov/kinetic/issues">Issues</a>
  ·
  <a href="LICENSE">License</a>
</p>

<br>

## Intro

KinetiC is **not** a continuation of our first try to build a web server ([cserve](github.com/Samandar-Komilov/cserve)). In cserve, we acted as a Product Owner with a set of features to implement, with time constraint. The features were implemented, the server worked, but it was not production ready, code was buggy, edge cases not handled. Later, when I explored RFCs deeper, I didn't met almost any of the official HTTP RFCs in cserve. That motivated me to rebuild a web server, this time fully following RFCs and invariant-based thinking. 

## Architecture

KinetiC is a web server written in C language, with CMake build system. The legendary language gives us build the system from scratch, understanding what is going on under the hood best. Also, HTTP server is a best way to practice invariant-based thinking with state machines. 

#### Concurrency model

Concurrency is handled with [libuv](https://libuv.org/), cross-platform asynchronous I/O, trusted by Node.js. 

- **libuv** — one `uv_loop_t` per worker process; non-blocking TCP accept/read/write
  on thousands of connections without one thread per client.
- **Multi-process** — master/worker split is application-level (`fork` or
  `SO_REUSEPORT`), not provided by libuv. Each worker runs its own isolated loop.
- **Memory** — per-connection `ktc_arena`; `ktc_arena_reset()` between keep-alive
  requests.

The following libuv APIs will help us throughout the development:

| Era | APIs |
|-----|------|
| 1 — connection | `uv_loop_*`, `uv_tcp_*`, `uv_listen`, `uv_accept`, `uv_read_start`, `uv_write`, `uv_close` |
| 3 — response | `uv_write` (response framing) |
| 5 — persistence | `uv_shutdown`, `uv_timer_*` (idle timeout), staged close |
| ops | `uv_signal_*` (graceful stop) |

#### Configuration

Configuration is in YAML, aimed to achieve cloud native status like Traefik, moving away from 2000's nginx's static configuration system. 

#### Dashboard

An interactive dashboard in port 8098 work when you run kinetic. It shows core stats of the web server: backends and their ports, requests count, throughput, error rate, etc. The dashboard APIs are also C-based, UI is pure HTML & CSS & JS.

## Installation

>[!NOTE]
>The app will be installable via popular linux package managers (e.g. `apt`/`dnf`) and docker, once we achieve 0.2.0 version.

## Layout

```
include/ktc/core/     ktc_str, ktc_arena
src/core/             implementations
src/main.c            your entry point grows here
configs/kinetic.yaml.example   minimal YAML (you parse in notebook 0 Era 1)
docs/notebooks/       evolution story
```

## Requirements

- CMake 3.20+, C17
- libuv / libyaml

## Build

```bash
make build
make test
./build/src/kinetic
```

## Format & lint

```bash
make format
make check
```

## License

MIT — see [LICENSE](LICENSE).
