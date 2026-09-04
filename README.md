<p align="center">
  <picture>
    <source media="(prefers-color-scheme: dark)"
            srcset="data/logo/kinetic-logo-dark.png">
    <img src="data/logo/kinetic-logo-light.png" height="72" alt="KinetiC">
  </picture>
</p>

<h1 align="center">KinetiC</h1>

<p align="center">
  High-performance, cloud-native <strong>HTTP/1.1</strong> server & edge proxy in C17 — built with RFC invariant-based state machines and libuv.
</p>

<p align="center">
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-MIT-blue.svg" alt="License: MIT"></a>
  <a href="https://github.com/Samandar-Komilov/kinetic"><img src="https://img.shields.io/badge/version-0.1.0-7c3aed" alt="Version 0.1.0"></a>
  <img src="https://img.shields.io/badge/status-in%20development-yellow" alt="In development">
  <img src="https://img.shields.io/badge/language-C17-00599C?logo=c&logoColor=white" alt="C17">
  <img src="https://img.shields.io/badge/HTTP-1.1-007EC6" alt="HTTP/1.1">
  <img src="https://img.shields.io/badge/RFC-9110%20%7C%209112-555" alt="RFC 9110 / 9112">
  <img src="https://img.shields.io/badge/I%2FO-libuv-403C3D?logo=libuv&logoColor=white" alt="libuv">
  <img src="https://img.shields.io/badge/deps-vcpkg-064F8C" alt="vcpkg">
  <img src="https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows-lightgrey" alt="Cross-platform">
</p>

<p align="center">
  <a href="docs/STATE.md">Project State</a>
  ·
  <a href="docs/invariants/HTTP_1_1.md">RFC Invariants Checklist</a>
  ·
  <a href="configs/example.yaml">Example config</a>
  ·
  <a href="LICENSE">License</a>
</p>

<br>

## Overview

**KinetiC** is a high-performance, cloud-native HTTP/1.1 web server and reverse proxy written in C17. It is designed from the ground up to achieve production-grade reliability by strictly implementing official IETF specifications (**RFC 9110** and **RFC 9112**) through deterministic state machines, non-owning memory slices, and zero-allocation parser design.

The ultimate goal of KinetiC is to serve as a fast, lightweight, cloud-native edge proxy (in the spirit of Traefik, but written in pure C), consuming Docker Unix domain sockets and Kubernetes API event streams for dynamic routing without server restarts.

---

## Architecture & Core Design

* **Asynchronous I/O**: Event-driven networking using [`libuv`](https://libuv.org/) for non-blocking TCP socket operations (`uv_tcp_t`), stream allocation callbacks, and signal handling.
* **Memory Management**: Fixed-size global slot pool for socket connections coupled with per-connection bump arenas (`ktc_arena`). Arenas are reset between requests on persistent connections to eliminate heap fragmentation.
* **Zero-Copy Wire Octets**: Wire data is parsed using `ktc_str` slices (`{const uint8_t *ptr, size_t len}`), avoiding unnecessary string duplications during protocol header decoding.
* **Multi-Worker Concurrency**: Scalable multi-process worker model leveraging `SO_REUSEPORT` to distribute TCP accepts cleanly across isolated `uv_loop_t` worker loops.
* **Cloud-Native Provider Engine**: Designed to read static YAML configurations (`src/core/parseyml.c`) or dynamically update route tables by watching `/var/run/docker.sock` and Kubernetes API Ingress resources.
* **Embedded Dashboard**: Integrated admin and observability engine running on port 8098 to visualize connection stats, request throughput, error rates, and backend health in real time.

---

## Main Development Phases

Below is the master roadmap of KinetiC's architectural milestones:

- [x] **Phase 1: Infrastructure & Basement Layer**
  - [x] C17 CMake build system and unit/integration test harness
  - [x] `libuv` event loop and non-blocking TCP server setup
  - [x] `libyaml` static configuration parsing engine
  - [x] Memory management primitives (`ktc_arena` bump allocator, `ktc_str` octet slice)
  - [x] `vcpkg` package manager integration (manifest mode)
  - [x] Global connection pool slot allocator (`ktc_connection_pool_init`)

- [ ] **Phase 2: HTTP/1.1 Wire Parsing & RFC Invariants** *(In Progress)*
  - [x] RFC 9112 §3 Request-line parser with grammar verification (`req_line.c`)
  - [x] RFC 9112 §5 Header field parser and Host validation (`headers.c`)
  - [x] RFC 9112 §6.3 Body framing precedence: `Content-Length` and `Transfer-Encoding: chunked` (`body.c`)
  - [x] RFC 9110 status response generation (`response.c`)
  - [ ] Strict enforcement of combined CL + TE request desynchronization guards

- [ ] **Phase 3: Connection Lifetime, Persistence & Pipelining**
  - [ ] RFC 9112 §9.3 Persistent connection state machine (`IDLE` state transition loop)
  - [ ] `Connection: close` request/response headers handling
  - [ ] Connection idle timeout management via `uv_timer_t`
  - [ ] RFC 9112 §9.3.2 FIFO response delivery queue for pipelined requests

- [ ] **Phase 4: HTTP Method Semantics & Protocol Extensions**
  - [ ] `HEAD` method response body suppression
  - [ ] `OPTIONS` method capability reporting (`Allow` header)
  - [ ] RFC 9110 §10.1.1 `Expect: 100-continue` intermediate status handling
  - [ ] RFC 9111 Conditional GET validation (`ETag`, `If-None-Match`, `304 Not Modified`)
  - [ ] RFC 9110 §8.4 Content Coding (`gzip` response compression)

- [ ] **Phase 5: Multi-Worker Process Architecture**
  - [ ] `SO_REUSEPORT` kernel socket load balancing across worker processes
  - [ ] Master-worker process lifecycle, IPC channels, and health monitoring
  - [ ] Graceful SIGINT/SIGTERM drain sequence across all active loops

- [ ] **Phase 6: Cloud-Native Interfacing (Docker & Kubernetes)**
  - [ ] Docker Unix Domain Socket listener (`/var/run/docker.sock`) for container events
  - [ ] Kubernetes API Server event watcher over TLS & JSON parsing (`yyjson`)
  - [ ] Lock-free atomic routing graph update mechanism

- [ ] **Phase 7: Observability & Interactive Dashboard Server**
  - [ ] Embedded metrics collector (request throughput, latency histograms, error rates)
  - [ ] Built-in HTTP admin server and interactive dashboard on port 8098

---

## Repository Structure

```text
include/ktc/core/     Public headers: str.h, arena.h, config.h, connection.h
include/ktc/http/     HTTP parser headers: req_line.h, headers.h, body.h, response.h
src/core/             Core implementations (str.c, arena.c, parseyml.c, connection.c)
src/http/             HTTP parsers (req_line.c, headers.c, body.c, response.c)
src/main.c            Main entry point & libuv loop initialization
configs/              YAML configuration files
docs/                 Documentation: STATE.md, INVARIANTS.md
tests/                Unit and integration test suites
vcpkg.json            vcpkg package manifest
```

---

## Build & Verify

### Requirements
* **CMake 3.20+**
* **C17 Compiler** (`gcc` or `clang`)
* **libuv** and **libyaml** (provided via system packages, vcpkg, or FetchContent)

### Build & Run
```bash
# Build binary
make build

# Run unit and integration tests
make check

# Start KinetiC server
./build/src/kinetic configs/test_config.yaml
```

---

## License

MIT — see [LICENSE](LICENSE).
