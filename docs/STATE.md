# Kinetic State machine

### Phase 0: Basement Layer & Infrastructure

[x] CMake build system, C17 standard enforcement, and test suite setup (`tests/`)
[x] libuv async I/O engine & event loop initialization (`src/main.c`)
[x] YAML configuration parsing engine (`src/core/parseyml.c` via libyaml)
[x] Memory management primitives (`ktc_arena` bump allocator, `ktc_str` wire octet slice)
[x] Dependency management via vcpkg manifest mode (`vcpkg.json`, `cmake/Dependencies.cmake`)

---

### Phase 1: Connection Management & Socket Lifecycle

##### Phase 1.1: Transport & Socket Accept Setup
[x] Fixed-size connection pool slot allocator (`ktc_connection_pool_init`, `src/core/connection.c`)
[x] RFC 9112 §9.6 (H11-LIFE-006): Staged graceful socket shutdown (`uv_shutdown`) and handle close (`uv_close` / `on_handle_closed`)
[x] RFC 9112 §9.3 (H11-CONN-002): Send `Connection: close` behavior after single request/response cycle (pre-persistence mode)

##### Phase 1.2: Global Lifecycle Rules & Persistence
[ ] RFC 9112 §9.2 / §9.3.2 (H11-LIFE-001): Connection carries ordered request/response pairs (pipelining ordering)
[ ] RFC 9110 §5.3 (H11-LIFE-002 / H11-HDR-005): Do not apply request to resource until full header section is received
[ ] RFC 9112 §9.3 (H11-LIFE-003): Read entire request body or close connection on persistent socket
[ ] RFC 9112 §9.3 (H11-LIFE-004): Require self-defined message length on persistent connection
[ ] RFC 9112 §9.6 (H11-LIFE-005): Process no further requests after sending or receiving `Connection: close`
[ ] RFC 9112 §9.3 (H11-CONN-001): Persistent connection state machine (return parser FSM to `IDLE` state after response completes)
[ ] RFC 9112 §9.6 (H11-CONN-003): On receiving `Connection: close`, close socket after response, echo `close`, process no further requests
[ ] RFC 9112 §9.6 (H11-CONN-004): If sending `Connection: close`, close socket after response, process no further requests
[ ] RFC 9112 §9.5 (H11-CONN-005): Sustain persistent connections; prefer flow control over aggressive close (idle timeout timers via `uv_timer_t`)
[ ] RFC 9112 §9.4 (H11-CONN-006): Reject abusive connection counts (DoS mitigation)

---

### Phase 2: HTTP/1.1 Request Parsing

##### Phase 2.1: Octet Stream & Line Endings
[x] RFC 9112 §2.2 (H11-PARSE-001): Parse messages strictly as octet stream in US-ASCII superset
[x] RFC 9112 §2.2 (H11-PARSE-003/007): Validate line endings; respond 400 Bad Request and close on syntax violations
[x] RFC 9112 §2.2 (H11-PARSE-006): Ignore empty CRLF preceding request-line
[ ] RFC 9112 §2.2 (H11-PARSE-002): Must not generate bare CR in protocol elements
[ ] RFC 9112 §2.2 (H11-PARSE-004/005): Must not send whitespace between start-line and first header; reject or consume if received

##### Phase 2.2: Request Line (`src/http/req_line.c`)
[x] RFC 9112 §3.2 (H11-REQLINE-001): Parse request line grammar `method SP request-target SP HTTP-version CRLF`
[x] RFC 9112 §3.2 (H11-REQLINE-002): Respond 414 URI Too Long when request-target exceeds limit (8192 octets)
[x] RFC 9112 §3.2 (H11-REQLINE-003): Respond 501 Not Implemented for unsupported HTTP methods
[x] RFC 9110 §6.2 (H11-STATUS-004): Respond 505 HTTP Version Not Supported when version major is not HTTP/1.x
[x] RFC 9112 §3.2 (H11-REQLINE-004): Reject invalid request-line with 400 Bad Request
[ ] RFC 9112 §3.2.1: Accept origin-form, absolute-form, authority-form (CONNECT), and asterisk-form (OPTIONS *)

##### Phase 2.3: Host Validation & Request Target (`src/http/headers.c`)
[x] RFC 9112 §3.2 / RFC 9110 §7.2 (H11-HOST-001): Validate Host header presence & format (must exist once; 400 Bad Request if missing/malformed)
[ ] RFC 9112 §3.2.3 (H11-HOST-002): Ignore received Host header on absolute-form target; use authority from target
[ ] RFC 9112 §3.2.3 (H11-HOST-003): Accept absolute-form even from direct clients
[ ] RFC 9110 §9.3.6 (H11-HOST-004): Reject CONNECT with empty/invalid port with 400 Bad Request
[ ] RFC 9110 §4.3.3 (H11-HOST-005): Reject https scheme requirements if not received over valid TLS
[ ] RFC 9112 §3.2.3 (H11-HOST-006): For empty authority on http/https URI: reject or apply configured default

##### Phase 2.4: Header Section (`src/http/headers.c`)
[x] RFC 9112 §5.1 (H11-HDR-001): Parse field lines `field-name ":" OWS field-value OWS`
[x] RFC 9112 §5.2 (H11-HDR-001 / H11-SEC-004): Reject whitespace between header name and colon with 400 Bad Request
[x] RFC 9112 §2.2 (H11-HDR-002): Reject obsolete line folding (`obs-fold`) with 400 Bad Request
[x] RFC 9110 §5.4 (H11-HDR-004): Respond 431 Request Header Fields Too Large when header section exceeds max size
[x] RFC 9110 §5.3 (H11-HDR-005): Do not apply request until full header section is received
[ ] RFC 9110 §5.5 (H11-HDR-003): On CR, LF, or NUL in field value: reject message or replace with SP
[ ] RFC 9110 §5.1 (H11-HDR-006): Ignore unrecognized header/trailer fields

##### Phase 2.5: Message Body Framing (`src/http/body.c`)
[x] RFC 9112 §6.3 (H11-FRAME-001): Implement RFC 9112 §6.3 body framing precedence rules
[x] RFC 9112 §6.3 (H11-FRAME-008): Parse `Content-Length` header for fixed-size bodies
[x] RFC 9112 §7.1 (H11-CHUNK-001): Parse and decode `Transfer-Encoding: chunked` (hex chunk size, payload, CRLF, terminal `0\r\n\r\n`)
[x] RFC 9112 §7.1.1 (H11-CHUNK-002): Ignore unrecognized chunk extensions
[x] RFC 9112 §7.1 (H11-CHUNK-003): Prevent overflow on large hex chunk-size values
[x] RFC 9112 §6.3 (H11-FRAME-003): Reject invalid or conflicting `Content-Length` values with 400 Bad Request
[x] RFC 9110 §15.5.14: Enforce 10MB payload size limit safeguard with 413 Content Too Large
[ ] RFC 9112 §6.3 (H11-FRAME-002): On request TE present, chunked not final: 400, close
[ ] RFC 9112 §6.3 (H11-FRAME-004): On both TE and CL in request: process per TE or reject; always close after response
[ ] RFC 9112 §6.2 (H11-FRAME-005): On HTTP/1.0 message with Transfer-Encoding: treat framing faulty, close after processing
[ ] RFC 9112 §6.2 (H11-FRAME-006): Must not send Content-Length in message that has Transfer-Encoding
[ ] RFC 9112 §6.3 (H11-FRAME-007): Prefer length-delimited or chunked responses over close-delimited
[ ] RFC 9112 §7.1 (H11-CHUNK-004): Treat chunk extension parameters as error
[ ] RFC 9112 §7.1.1 (H11-CHUNK-005): Limit total chunk-extension length; 4xx if exceeded
[ ] RFC 9112 §6.1 (H11-CHUNK-006): Must not send Transfer-Encoding on response unless request indicates HTTP/1.1+
[ ] RFC 9112 §6.1 (H11-CHUNK-007): Respond 501 to transfer coding not understood
[ ] RFC 9112 §8 (H11-INCOMPLETE-001): Send error response before close on incomplete request
[ ] RFC 9112 §8 (H11-INCOMPLETE-002): Treat chunked body incomplete until zero-size chunk received
[ ] RFC 9112 §8 (H11-INCOMPLETE-003): Treat CL-delimited body incomplete if fewer octets received than CL

---

### Phase 3: Response Generation & Status Line Formatting (`src/http/response.c`)

[x] RFC 9112 §4 (H11-STATUS-001): Format status-line with SP between status code and reason phrase
[x] RFC 9110 status codes: Format 200 OK, 400 Bad Request, 413 Content Too Large, 414 URI Too Long, 431 Fields Too Large, 501 Not Implemented, 505 Version Not Supported responses
[ ] RFC 9110 §6.2 (H11-STATUS-002): Must not send HTTP version in responses that server is not conformant with
[ ] RFC 9110 §6.2 (H11-STATUS-003): Response version = highest conformant version <= request major
[ ] RFC 9110 §15.2 (H11-STATUS-005): Must not send 1xx response to HTTP/1.0 client
[ ] RFC 9110 §6.6.1 (H11-HDR-007): Generate `Date` header on all 2xx, 3xx, 4xx responses
[ ] RFC 9110 §6.6.1 (H11-HDR-008): Generate `Date` on 1xx and 5xx responses
[ ] RFC 9110 §10.2.1 (H11-HDR-009): Generate `Allow` header on 405 Method Not Allowed
[ ] RFC 9110 §11.6.1 (H11-HDR-010): Generate `WWW-Authenticate` header on 401 Unauthorized
[ ] RFC 9110 §8.6 / RFC 9112 §6.1 (H11-FRAME-009): Must not send Content-Length on 1xx or 204 responses
[ ] RFC 9112 §6.1 (H11-FRAME-010): Must not send Transfer-Encoding on 1xx or 204 responses
[ ] RFC 9110 §9.3.6 / RFC 9112 §6.1 (H11-FRAME-011): Must not send Content-Length or Transfer-Encoding on 2xx CONNECT response
[ ] RFC 9110 §9.3.2 (H11-FRAME-012): Must not send content in HEAD response
[ ] RFC 9110 §15.3.6 (H11-FRAME-013): Must not generate content in 205 response

---

### Phase 4: Persistence & Pipelining State Machine

[ ] RFC 9112 §9.3.2 (H11-PIPE-001): Send pipelined responses in exact same order as requests received (FIFO response queue)
[ ] RFC 9112 §9.3.2 (H11-PIPE-002): Process safe pipelined requests in parallel
[ ] Parser FSM contract H11-FSM-001: REQUEST_HEADERS -> DISPATCH without Host check forbidden
[ ] Parser FSM contract H11-FSM-002: REQUEST_BODY -> DISPATCH with partial body on persistent connection forbidden
[ ] Parser FSM contract H11-FSM-003: RESPONSE -> IDLE requires response message complete per §6.3
[ ] Parser FSM contract H11-FSM-004: Any unrecoverable framing error -> emit error response, then CLOSING
[ ] Parser FSM contract H11-FSM-005: After CLOSING initiated for smuggling/framing error, do not read another request on same connection

---

### Phase 5: HTTP Method Semantics & Protocol Extensions

##### Phase 5.1: Method Semantics
[ ] RFC 9110 §9.3.2 (H11-SEM-001): `HEAD` method semantics (generate identical headers as GET, suppress response body)
[ ] RFC 9110 §9.3.4 (H11-SEM-002): `PUT` creates resource -> 201; updates -> 200 or 204
[ ] RFC 9110 §9.3.6 (H11-SEM-003): `CONNECT` method handling & invalid port rejection -> 400
[ ] RFC 9110 §3.5 (H11-SEM-004): Must not assume two requests on same connection are same user agent

##### Phase 5.2: Expect & Preconditions
[ ] RFC 9110 §10.1.1 (H11-EXPECT-001): Handle `Expect: 100-continue` (send 100 Continue or 4xx before reading body)
[ ] RFC 9110 §10.1.1 (H11-EXPECT-002): Must not wait for body before sending 100 Continue
[ ] RFC 9110 §7.8 (H11-EXPECT-003): If Upgrade + 100-continue send 100 Continue before 101 Switching Protocols
[ ] RFC 9110 §13.1 (H11-COND-001): Evaluate `If-Match` before method (strong comparison)
[ ] RFC 9110 §13.1 (H11-COND-002): Evaluate `If-None-Match`; false -> 304 (GET/HEAD) or 412 (others)
[ ] RFC 9110 §13.2 (H11-COND-003): Evaluate preconditions in order defined §13.2
[ ] RFC 9110 §13.1.5 (H11-COND-004): Ignore `If-Range` when no `Range` header
[ ] RFC 9110 §14.1 (H11-COND-005): Ignore `Range` for methods other than GET

##### Phase 5.3: Upgrade, Range & Content Codings
[ ] RFC 9110 §7.8 / §15.2.2 (H11-UPG-001): 101 response includes `Upgrade` header listing new protocols
[ ] RFC 9110 §7.8 (H11-UPG-002): Must not switch to protocol not indicated in client `Upgrade`
[ ] RFC 9110 §7.8 (H11-UPG-003): 426 response includes `Upgrade` header with acceptable protocols
[ ] RFC 9110 §7.8 (H11-UPG-004): Must not switch unless new protocol can honor received message semantics
[ ] RFC 9110 §15.3.7 (H11-RANGE-001): 206 response with multiple parts: `multipart/byteranges` + per-part `Content-Range`
[ ] RFC 9110 §15.3.7 (H11-RANGE-002): Must not send `Content-Range` in header of multipart 206 (only in parts)
[ ] RFC 9110 §8.8.2 (H11-VAL-001): `Last-Modified` must not be later than message `Date`
[ ] RFC 9110 §8.8.3 (H11-VAL-002): Prefix weak entity tags with `W/` when not strong validators
[ ] RFC 9110 §8.4: Content Coding (compress response body with gzip when `Accept-Encoding: gzip` present)

---

### Phase 6: Security & Desynchronization Guards

[x] RFC 9112 §5.2 (H11-SEC-004): Reject header field name space before colon (`400 Bad Request`)
[ ] RFC 9112 §11.2 (H11-SEC-001): Single consistent parsing algorithm across requests on connection
[ ] RFC 9112 §11.1 (H11-SEC-002): Prevent response splitting (sanitize CR/LF in generated headers)
[ ] RFC 9112 §6.3 / §11.2 (H11-SEC-003): Request smuggling protection (strict framing, reject ambiguous TE/CL)
[ ] RFC 9110 §5.4 (H11-SEC-005): Must not ignore oversize headers (reject with 431)

---

### Phase 7: TLS / HTTPS Transport

[ ] RFC 9112 §9.7 (H11-TLS-001): Send all HTTP data as TLS application data
[ ] RFC 9112 §9.8 (H11-TLS-002): Attempt TLS closure alert exchange before socket closure
[ ] RFC 9112 §9.8 (H11-TLS-003): Handle client incomplete close using HTTP framing
[ ] RFC 9110 §4.3.3 (H11-TLS-004): Reject https scheme requirements on cleartext connection

---

### Phase 8: Multi-Worker OS Architecture

[ ] Kernel socket load balancing using `SO_REUSEPORT` across worker processes
[ ] Isolated `uv_loop_t` event loop execution per worker process
[ ] Master process manager: worker fork, health check monitoring, and non-blocking IPC channels
[ ] Graceful signal handling (`SIGINT`/`SIGTERM`/`SIGHUP`) with connection draining

---

### Phase 9: Cloud-Native Edge Proxy & Dynamic Routing

[ ] Unix Domain Socket listener (`uv_pipe_t`) connecting to `/var/run/docker.sock` for container lifecycle events
[ ] Kubernetes API watcher over TLS & JSON parsing (`yyjson` via vcpkg) monitoring Ingress/CRD resource streams
[ ] Lock-free, atomic routing table swap mechanism
[ ] Reverse proxy forwarding engine with connection pooling to upstream backends

---

### Phase 10: Observability & Dashboard Server

[ ] In-memory stats collector (request throughput, latency histograms, error counts, active connection count)
[ ] Embedded HTTP administration API & interactive dashboard web UI running on port 8098
