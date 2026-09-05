# Architectural Decisions & Technical Choices

Key design choices, security invariants, and technical decisions made in KinetiC.

---

## 1. Basement Layer & Infrastructure

### 1.1 Zero-Copy Octet Slices (`ktc_str`) & Bump Arenas (`ktc_arena`)
* **Problem**: Heap allocations per header/token (`strndup`, `malloc`) cause heavy allocator overhead and memory fragmentation under high request volume.
* **Decision**: Combine non-owning octet slices `ktc_str` with per-connection bump allocation `ktc_arena`.
* **Rationale**:
  * `ktc_str` (`{const uint8_t *ptr, size_t len}`) views incoming network buffers directly without string copying.
  * `ktc_arena` provides fast bump allocation for per-request structures, reset in $O(1)$ time via `ktc_arena_reset()` between keep-alive requests.
* **References**:
  * [`include/ktc/core/str.h`](../include/ktc/core/str.h) & [`src/core/str.c`](../src/core/str.c) — Non-owning wire slice primitive and string utilities.
  * [`include/ktc/core/arena.h`](../include/ktc/core/arena.h) & [`src/core/arena.c`](../src/core/arena.c) — Bump arena allocator implementation.
  * [`tests/unit/basement/test_str.c`](../tests/unit/basement/test_str.c) & [`tests/unit/basement/test_arena.c`](../tests/unit/basement/test_arena.c) — Unit test suites.

### 1.2 `libuv` Async Engine & Reference-Counted Handle Teardown
* **Problem**: Direct Linux `epoll` code (`fcntl O_NONBLOCK`) is non-portable (fails on macOS/Windows), uses static connection arrays that drop traffic bursts, and risks use-after-free bugs during socket destruction.
* **Decision**: Use `libuv` (`uv_tcp_t`, `uv_loop_t`) with reference-counted connection contexts (`pending_closes_cnt`).
* **Rationale**:
  * Cross-platform event loop: Compiles to `epoll` on Linux, `kqueue` on macOS, and IOCP on Windows.
  * Dynamic allocation: Context structures (`ktc_conn_t`) scale dynamically with active connection volumes.
  * Memory safety: `pending_closes_cnt` ensures `ktc_conn_t` and arena are freed only after all async handle close callbacks (`on_handle_closed`) complete.
* **References**:
  * [`src/main.c`](../src/main.c) — Main entry point, default loop initialization, and TCP listener setup.
  * [`include/ktc/core/connection.h`](../include/ktc/core/connection.h) & [`src/core/connection.c`](../src/core/connection.c) — Connection context lifecycle and `pending_closes_cnt` management.

### 1.3 vcpkg Manifest Mode for Cloud-Native Dependencies
* **Problem**: C third-party libraries (`libuv`, `libyaml`, future `openssl`, `yyjson`) vary across Linux distros and platforms.
* **Decision**: Integrate `vcpkg` manifest mode (`vcpkg.json`) with CMake (`cmake/Dependencies.cmake`).
* **Rationale**:
  * Ensures reproducible builds across local machines and containerized CI/CD builds.
  * Maintains fallback compatibility with system `pkg-config` and CMake `FetchContent`.
* **References**:
  * [`vcpkg.json`](../vcpkg.json) — Declared package manifest (`libuv`, `libyaml`).
  * [`cmake/Dependencies.cmake`](../cmake/Dependencies.cmake) & [`CMakeLists.txt`](../CMakeLists.txt) — Dependency resolution logic.

---

## 2. Connection Management & Socket Lifecycle

### 2.1 Staged Socket Teardown & TCP Write Half-Close (`uv_shutdown`)
* **Problem**: When a server finishes sending an HTTP response in pre-persistence mode (or after handling an error), closing the socket abruptly via `close(fd)` / `uv_close()` causes the operating system kernel to issue a **TCP Reset (`RST`)** packet if unread client data or unacknowledged in-flight packets remain in the socket queues. A TCP `RST` forces the client OS to discard its local receive buffer immediately, destroying the HTTP response before the client application can read it (resulting in `ECONNRESET` / `Connection reset by peer`).
* **Decision**: Implement a two-stage asynchronous teardown pipeline using `ktc_write_req_t`, `uv_shutdown()`, and `uv_close()` per RFC 9112 §9.6 (`H11-LIFE-006`):
  1. **Asynchronous Write Tracking (`ktc_write_req_t`)**: Wrap the `uv_write_t` handle, response payload buffer (`base`), and connection owner (`conn`) together. Because `uv_write()` is non-blocking, response buffer memory is preserved until libuv triggers the `on_write()` completion callback.
  2. **Write Channel Half-Close (`uv_shutdown()`)**: After `uv_write` completes in `on_write()`, the server calls `uv_shutdown()` on the client TCP stream. This transmits a clean TCP `FIN` packet to the client, indicating that the server has finished writing data, while keeping the socket open to drain in-flight ACKs cleanly.
  3. **Handle Destruction (`uv_close()`)**: Only once the shutdown handshake finishes and libuv invokes `on_shutdown()` does the server initiate socket destruction via `conn_close()` -> `uv_close()`, safely recycling memory in `on_handle_closed()`.
* **Rationale**:
  * Prevents connection resets (`ECONNRESET`) and guarantees delivery of the final response (e.g. `200 OK`, `400 Bad Request`, `414 URI Too Long`, `431 Request Header Fields Too Large`).
  * Conforms strictly to RFC 9112 §9.6 staged shutdown requirements.
  * Disables Nagle's algorithm (`uv_tcp_nodelay`) to eliminate delivery latency for response packets.
* **References**:
  * [`src/core/connection.c`](../src/core/connection.c) — `ktc_write_req_t`, `send_response()`, `on_write()`, `on_shutdown()`, `conn_close()`, `on_handle_closed()`, `ktc_on_connection()`.
  * [`include/ktc/http/response.h`](../include/ktc/http/response.h) & [`src/http/response.c`](../src/http/response.c) — `ktc_response_format_empty()` injecting `Connection: close`.
  * [`tests/unit/http/test_response.c`](../tests/unit/http/test_response.c) — Response format and `Connection: close` assertions.
  * [`tests/integration/test_connection_handling.py`](../tests/integration/test_connection_handling.py) — Live integration verification (`test_staged_shutdown_half_close`, `test_single_cycle_closes_socket`).

### 2.2 Global Multiplexed (Non-Owned) Connection Buffer Pool
* **Problem**: Before processing an incoming request, the server must provide libuv with a buffer (`uv_alloc_cb`) to read incoming TCP octets. Managing this memory went through three distinct design iterations:
  1. **Iteration 1 — Malloc on Every Read Chunk**: Allocated a temporary buffer (`malloc`) on every incoming network chunk and freed it after reading.
     * *Drawback*: Severe memory allocator churn, system call overhead, and heap fragmentation under heavy I/O.
  2. **Iteration 2 — Connection-Owned Static Slot**: Each connection permanently held its own dedicated buffer slot for its entire lifetime.
     * *Drawback*: Holding dedicated memory for thousands of idle/keep-alive connections wastes RAM and caps maximum concurrent connections strictly to the pool size.
  3. **Iteration 3 (Current) — Global Multiplexed Pool with On-Demand Borrowing**: A pre-allocated global pool (`KTC_MAX_CONNECTIONS * KTC_GLOBAL_POOL_SLOT_SIZE`) where connections borrow a slot (`borrow_free_slot`) only when bytes actively arrive in `on_alloc`, and release it (`release_conn_borrowed_slot`) immediately once request parsing completes or the socket closes.
* **Rationale**: Decouples the number of open connections from the active memory footprint, allowing high concurrency while completely eliminating per-chunk `malloc`/`free` calls.
* **References**:
  * [`include/ktc/core/connection.h`](../include/ktc/core/connection.h) — Global pool size constants (`KTC_MAX_CONNECTIONS`, `KTC_GLOBAL_POOL_SLOT_SIZE`), `ktc_connection_pool_init()`, `ktc_connection_pool_destroy()`.
  * [`src/core/connection.c`](../src/core/connection.c) — Global slot state array (`global_pool_slot_states`), `borrow_free_slot()`, `release_conn_borrowed_slot()`, `on_alloc()`, `on_read()`.
  * [`tests/integration/test_connection_handling.py`](../tests/integration/test_connection_handling.py) — Concurrent client pool test (`test_connection_pool_concurrency`).

---

## 3. HTTP/1.1 Request Parsing

### 3.1 Incremental Request Line FSM (`src/http/req_line.c`)
* **Problem**: Naive `memchr`/`strstr` scanning assumes full request strings exist contiguously in memory, breaking on split TCP packets or allowing double spaces (`GET  /`).
* **Decision**: Character-by-character incremental Finite State Machine (`ktc_req_line_parser_feed`).
* **Rationale**:
  * Resumes parsing across split TCP packets without buffer reallocation.
  * Skips leading empty CRLF lines before request-line per RFC 9112 §2.2.
  * Strictly rejects double spaces, tabs, or control characters immediately with `400 Bad Request` to prevent request smuggling.
  * Enforces token length bounds: URI length capped at 8KB (`414 URI Too Long`), method length capped at 32B (`501 Not Implemented`).
* **References**:
  * [`include/ktc/http/req_line.h`](../include/ktc/http/req_line.h) — Parser states (`ktc_req_line_state_t`), error codes (`ktc_req_line_err_t`), and struct definitions.
  * [`src/http/req_line.c`](../src/http/req_line.c) — Incremental FSM implementation (`ktc_req_line_parser_feed`, `ktc_req_line_parser_verify`).
  * [`tests/unit/http/test_req_line.c`](../tests/unit/http/test_req_line.c) — Unit test suite covering RFC 9112 §3.2, 8192B limits, methods, target forms, versions, and 1-byte streaming feeds.
  * [`tests/integration/test_request_line.py`](../tests/integration/test_request_line.py) — Integration test suite for request line and line ending violations.

### 3.2 Strict Header Delimiters & Host Invariants (`src/http/headers.c`)
* **Problem**: Lenient header parsing (accepting space before colon, obsolete line folding, control characters, missing `Host`) causes proxy desynchronization attacks.
* **Decision**: Enforce strict RFC 9112 header syntax rules in `ktc_header_parser_feed` and `resolve_and_validate`.
* **Rationale**:
  * **Space before colon** (`Host : val`): Triggers immediate `400 Bad Request` (`H11-HDR-001`, `H11-SEC-004`).
  * **Obsolete line folding (`obs-fold`)**: Rejects tabs/spaces following CRLF with `400 Bad Request` (`H11-HDR-002`).
  * **Control Characters**: Rejects `CR`, `LF`, or `NUL` inside header values (`H11-HDR-003`).
  * **Host Header**: Mandates `Host` presence on HTTP/1.1 requests, rejects duplicate `Host` headers (`H11-HOST-001`), and extracts authority from absolute URIs (`H11-HOST-002`).
  * **Size Cap**: Enforces a strict 16KB header block size limit (`431 Request Header Fields Too Large` via `H11-HDR-004`).
* **References**:
  * [`include/ktc/http/headers.h`](../include/ktc/http/headers.h) — Header structs (`ktc_header_t`), parser FSM states (`ktc_header_state_t`), error types (`ktc_header_err_t`).
  * [`src/http/headers.c`](../src/http/headers.c) — Incremental header FSM, OWS trimming, obs-fold rejection, and Host validation (`ktc_header_parser_feed`, `ktc_header_parser_resolve_and_validate`).
  * [`tests/unit/http/test_headers.c`](../tests/unit/http/test_headers.c) — Unit test suite covering RFC 9112 §5.1, §5.2, obs-fold, 16KB limits, Host validation, and byte-by-byte streaming.
  * [`tests/integration/test_headers.py`](../tests/integration/test_headers.py) — Integration test suite for headers and host validation invariants.

### 3.3 Framing Precedence (RFC 9112 §6.3) & Safe Body Parsing (`src/http/body.c`)
* **Problem**: Unsafe `atoi` conversions on `Content-Length` lead to integer overflows/undefined behavior. Ignoring `Transfer-Encoding` when `Content-Length` is present enables CL.TE / TE.CL smuggling attacks. Unhandled chunked markers corrupt body payloads.
* **Decision**: Implement strict §6.3 framing precedence logic (`ktc_body_resolve_framing`).
* **Rationale**:
  * **Smuggling Protection**: Rejects requests containing **both** `Content-Length` and `Transfer-Encoding` (`H11-SEC-003`, `H11-FRAME-004`).
  * **Safe Integer Conversion**: Validates that `Content-Length` contains exclusively digits and guards against integer overflow (`H11-FRAME-003`).
  * **In-Place Chunked Decoding**: Decodes `Transfer-Encoding: chunked` in-place (`H11-CHUNK-001`), ignores unrecognized chunk extensions (`H11-CHUNK-002`), and checks hex chunk-size bounds (`H11-CHUNK-003`).
  * **Payload Safeguard**: Enforces a 10MB request payload limit (`413 Content Too Large`).
* **References**:
  * [`include/ktc/http/body.h`](../include/ktc/http/body.h) — Body framing enum (`ktc_body_framing_t`), chunk decoder FSM (`ktc_chunk_state_t`), parser state (`ktc_body_parser_t`).
  * [`src/http/body.c`](../src/http/body.c) — RFC 9112 §6.3 framing resolution (`ktc_body_resolve_framing`), chunked decoder (`ktc_chunk_parser_feed`), and payload feeding.
  * [`tests/unit/http/test_body.c`](../tests/unit/http/test_body.c) — Unit test suite covering framing precedence, Content-Length, chunked encoding, trailers, and hex overflow protection.
  * [`tests/integration/test_connection_handling.py`](../tests/integration/test_connection_handling.py) — Live integration tests for Content-Length, chunked bodies, and smuggling rejection.
