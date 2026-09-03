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

### 1.2 `libuv` Async Engine & Reference-Counted Handle Teardown
* **Problem**: Direct Linux `epoll` code (`fcntl O_NONBLOCK`) is non-portable (fails on macOS/Windows), uses static connection arrays that drop traffic bursts, and risks use-after-free bugs during socket destruction.
* **Decision**: Use `libuv` (`uv_tcp_t`, `uv_loop_t`) with reference-counted connection contexts (`pending_closes_cnt`).
* **Rationale**:
  * Cross-platform event loop: Compiles to `epoll` on Linux, `kqueue` on macOS, and IOCP on Windows.
  * Dynamic allocation: Context structures (`ktc_conn_t`) scale dynamically with active connection volumes.
  * Memory safety: `pending_closes_cnt` ensures `ktc_conn_t` and arena are freed only after all async handle close callbacks (`on_handle_closed`) complete.

### 1.3 vcpkg Manifest Mode for Cloud-Native Dependencies
* **Problem**: C third-party libraries (`libuv`, `libyaml`, future `openssl`, `yyjson`) vary across Linux distros and platforms.
* **Decision**: Integrate `vcpkg` manifest mode (`vcpkg.json`) with CMake (`cmake/Dependencies.cmake`).
* **Rationale**:
  * Ensures reproducible builds across local machines and containerized CI/CD builds.
  * Maintains fallback compatibility with system `pkg-config` and CMake `FetchContent`.

---

## 2. Connection Management & Socket Lifecycle

### 2.1 Transport Setup & Staged Socket Teardown
* **Problem**: Abrupt socket closure causes TCP RST packets that wipe unread response bytes at the client.
* **Decision**: Enable `uv_tcp_nodelay` to disable Nagle's algorithm for low latency, and execute staged shutdown (`uv_shutdown` write half-close followed by `uv_close`).

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

### 3.2 Strict Header Delimiters & Host Invariants (`src/http/headers.c`)
* **Problem**: Lenient header parsing (accepting space before colon, obsolete line folding, control characters, missing `Host`) causes proxy desynchronization attacks.
* **Decision**: Enforce strict RFC 9112 header syntax rules in `ktc_header_parser_feed` and `resolve_and_validate`.
* **Rationale**:
  * **Space before colon** (`Host : val`): Triggers immediate `400 Bad Request` (`H11-HDR-001`).
  * **Obsolete line folding (`obs-fold`)**: Rejects tabs/spaces following CRLF with `400 Bad Request` (`H11-HDR-002`).
  * **Control Characters**: Rejects `CR`, `LF`, or `NUL` inside header values (`H11-HDR-003`).
  * **Host Header**: Mandates `Host` presence on HTTP/1.1 requests, rejects duplicate `Host` headers (`H11-HOST-001`), and extracts authority from absolute URIs (`H11-HOST-002`).
  * **Size Cap**: Enforces a strict 16KB header block size limit (`431 Request Header Fields Too Large` via `H11-HDR-004`).

### 3.3 Framing Precedence (RFC 9112 §6.3) & Safe Body Parsing (`src/http/body.c`)
* **Problem**: Unsafe `atoi` conversions on `Content-Length` lead to integer overflows/undefined behavior. Ignoring `Transfer-Encoding` when `Content-Length` is present enables CL.TE / TE.CL smuggling attacks. Unhandled chunked markers corrupt body payloads.
* **Decision**: Implement strict §6.3 framing precedence logic (`ktc_body_resolve_framing`).
* **Rationale**:
  * **Smuggling Protection**: Rejects requests containing **both** `Content-Length` and `Transfer-Encoding` (`H11-SEC-003`, `H11-FRAME-004`).
  * **Safe Integer Conversion**: Validates that `Content-Length` contains exclusively digits and guards against integer overflow (`H11-FRAME-003`).
  * **In-Place Chunked Decoding**: Decodes `Transfer-Encoding: chunked` in-place (`H11-CHUNK-001`), ignores unrecognized chunk extensions (`H11-CHUNK-002`), and checks hex chunk-size bounds (`H11-CHUNK-003`).
  * **Payload Safeguard**: Enforces a 10MB request payload limit (`413 Content Too Large`).
