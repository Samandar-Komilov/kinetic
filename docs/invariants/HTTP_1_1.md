# HTTP/1.1 Server Invariants

This document is the **normative checklist** for KinetiC’s HTTP/1.1 implementation. Every item is derived from the official IETF specifications — not from nginx/Apache behavior, blog posts, or llhttp internals. When in doubt, the RFC text wins.

## Canonical references

| Document | Role |
|----------|------|
| [RFC 9110](https://www.rfc-editor.org/rfc/rfc9110.html) | HTTP Semantics (methods, status codes, fields, content) |
| [RFC 9112](https://www.rfc-editor.org/rfc/rfc9112.html) | HTTP/1.1 message syntax, framing, connection management |
| [RFC 9117](https://www.rfc-editor.org/rfc/rfc9117.html) | `Expect: 100-continue` |
| [RFC 2119](https://www.rfc-editor.org/rfc/rfc2119.html) / [RFC 8174](https://www.rfc-editor.org/rfc/rfc8174.html) | Meaning of MUST, SHOULD, MAY |

*(Note: HTTP Caching is defined separately under [CACHING.md](CACHING.md) per RFC 9111).*

**Obsoleted but historically relevant:** RFC 7230–7235, RFC 2616 (superseded by the 911x series, June 2022).

### Normative keywords

- **MUST / MUST NOT** — non-negotiable for a conformant server.
- **SHOULD / SHOULD NOT** — strong default; deviate only with documented reason.
- **MAY** — optional; listed when it affects interoperability or security posture.

Each invariant below has an **ID** (`H11-…`) for cross-linking in future topic documents.

---

## Connection lifecycle (implementation model)

RFC 9112 does not name a “state machine,” but a conformant HTTP/1.1 server on a single TCP connection can be modeled as sequential phases. **Invariants hold per phase and across phase transitions.**

```text
                    ┌──────────────┐
         accept ──► │    IDLE      │◄────────────────┐
                    └──────┬───────┘                 │
                           │ octets available         │ message complete
                           ▼                          │
                    ┌──────────────┐                 │
                    │ REQUEST_LINE │                 │
                    └──────┬───────┘                 │
                           ▼                          │
                    ┌──────────────┐                 │
                    │   HEADERS    │                 │
                    └──────┬───────┘                 │
                           │ framing known           │
                           ▼                          │
                    ┌──────────────┐   no body         │
                    │ REQUEST_BODY │───────────────────┤
                    └──────┬───────┘                 │
                           │ body complete           │
                           ▼                          │
                    ┌──────────────┐                 │
                    │  DISPATCH    │  (semantics)      │
                    └──────┬───────┘                 │
                           ▼                          │
                    ┌──────────────┐                 │
                    │   RESPONSE   │───────────────────┘
                    └──────┬───────┘
                           │ close option / error / HTTP/1.0
                           ▼
                    ┌──────────────┐
                    │   CLOSING    │
                    └──────────────┘
```

**Global lifecycle rules**

| ID | Level | Invariant | RFC |
|----|-------|-----------|-----|
| H11-LIFE-001 | MUST | A connection carries **ordered** request/response pairs; responses on a connection MUST correspond to requests in the **same order** received (pipelining). | [9112 §9.2](https://www.rfc-editor.org/rfc/rfc9112.html#section-9.2), [9112 §9.3.2](https://www.rfc-editor.org/rfc/rfc9112.html#section-9.3.2) |
| H11-LIFE-002 | MUST | Do **not** apply a request to a resource until the **entire request header section** has been received. | [9110 §5.3](https://www.rfc-editor.org/rfc/rfc9110.html#section-5.3) |
| H11-LIFE-003 | MUST | On a persistent connection, after sending a response, either **read the entire request body** of the current request or **close** the connection. | [9112 §9.3](https://www.rfc-editor.org/rfc/rfc9112.html#section-9.3) |
| H11-LIFE-004 | MUST | Every message on a persistent connection MUST have a **self-defined length** (not connection-close delimited), except allowed cases in §6.3. | [9112 §9.3](https://www.rfc-editor.org/rfc/rfc9112.html#section-9.3) |
| H11-LIFE-005 | MUST NOT | After sending or honoring `Connection: close`, process **no further requests** on that connection. | [9112 §9.6](https://www.rfc-editor.org/rfc/rfc9112.html#section-9.6) |
| H11-LIFE-006 | SHOULD | Close connections in **stages** (half-close write, drain read) to avoid TCP RST wiping the final response. | [9112 §9.6](https://www.rfc-editor.org/rfc/rfc9112.html#section-9.6) |

---

## 1. Message parsing and grammar

### 1.1 Octet stream and line endings

| ID | Level | Invariant | RFC |
|----|-------|-----------|-----|
| H11-PARSE-001 | MUST | Parse messages as a sequence of **octets** in a superset of US-ASCII — never as a Unicode character stream. | [9112 §2.2](https://www.rfc-editor.org/rfc/rfc9112.html#section-2.2) |
| H11-PARSE-002 | MUST NOT | Generate bare CR (CR not followed by LF) in protocol elements except message content. | [9112 §2.2](https://www.rfc-editor.org/rfc/rfc9112.html#section-2.2) |
| H11-PARSE-003 | MUST | Treat received bare CR as invalid **or** replace each with SP before processing. | [9112 §2.2](https://www.rfc-editor.org/rfc/rfc9112.html#section-2.2) |
| H11-PARSE-004 | MUST NOT | Send whitespace between start-line and first header field. | [9112 §2.2](https://www.rfc-editor.org/rfc/rfc9112.html#section-2.2) |
| H11-PARSE-005 | MUST | On whitespace between start-line and first header: **reject** or consume/ignore lines per §2.2 (smuggling mitigation). | [9112 §2.2](https://www.rfc-editor.org/rfc/rfc9112.html#section-2.2) |
| H11-PARSE-006 | SHOULD | Ignore at least one empty line (CRLF) before the request-line. | [9112 §2.2](https://www.rfc-editor.org/rfc/rfc9112.html#section-2.2) |
| H11-PARSE-007 | SHOULD | On grammar violation (outside robustness exceptions): respond **400** and **close** connection. | [9112 §2.2](https://www.rfc-editor.org/rfc/rfc9112.html#section-2.2) |

### 1.2 Request-line

| ID | Level | Invariant | RFC |
|----|-------|-----------|-----|
| H11-REQLINE-001 | MUST | Accept **origin-form**, **absolute-form**, **authority-form** (CONNECT), and **asterisk-form** (OPTIONS `*`). | [9112 §3.2](https://www.rfc-editor.org/rfc/rfc9112.html#section-3.2) |
| H11-REQLINE-002 | MUST | Respond **414 (URI Too Long)** if request-target exceeds server limit. | [9112 §3.2](https://www.rfc-editor.org/rfc/rfc9112.html#section-3.2), [9110 §15.5.15](https://www.rfc-editor.org/rfc/rfc9110.html#section-15.5.15) |
| H11-REQLINE-003 | SHOULD | Respond **501 (Not Implemented)** for method longer than any implemented method. | [9112 §3.2](https://www.rfc-editor.org/rfc/rfc9112.html#section-3.2) |
| H11-REQLINE-004 | SHOULD | On invalid request-line: **400** or **301** with properly encoded target — do **not** silently autocorrect. | [9112 §3.2](https://www.rfc-editor.org/rfc/rfc9112.html#section-3.2) |
| H11-REQLINE-005 | MAY | Lenient whitespace parsing in request-line — **discouraged** (smuggling risk if inconsistent with other parsers). | [9112 §3.2](https://www.rfc-editor.org/rfc/rfc9112.html#section-3.2) |

### 1.3 Status-line (responses the server sends)

| ID | Level | Invariant | RFC |
|----|-------|-----------|-----|
| H11-STATUS-001 | MUST | Send the SP between status-code and reason-phrase even when reason-phrase is empty. | [9112 §4](https://www.rfc-editor.org/rfc/rfc9112.html#section-4) |
| H11-STATUS-002 | MUST NOT | Send an HTTP version in responses that the server is not conformant with. | [9110 §6.2](https://www.rfc-editor.org/rfc/rfc9110.html#section-6.2) |
| H11-STATUS-003 | SHOULD | Response version = highest conformant version with major ≤ request major. | [9110 §6.2](https://www.rfc-editor.org/rfc/rfc9110.html#section-6.2) |
| H11-STATUS-004 | MAY | Send **505 (HTTP Version Not Supported)** to refuse client major version. | [9110 §6.2](https://www.rfc-editor.org/rfc/rfc9110.html#section-6.2) |
| H11-STATUS-005 | MUST NOT | Send **1xx** response to an HTTP/1.0 client. | [9110 §15.2](https://www.rfc-editor.org/rfc/rfc9110.html#section-15.2) |

---

## 2. Host and request-target

| ID | Level | Invariant | RFC |
|----|-------|-----------|-----|
| H11-HOST-001 | MUST | Respond **400** to HTTP/1.1 request **without** `Host`, with **multiple** `Host` field lines, or with **invalid** `Host` value. | [9112 §3.2](https://www.rfc-editor.org/rfc/rfc9112.html#section-3.2), [9110 §7.2](https://www.rfc-editor.org/rfc/rfc9110.html#section-7.2) |
| H11-HOST-002 | MUST | On **absolute-form** request-target: **ignore** received `Host`; use authority from request-target. | [9112 §3.2.3](https://www.rfc-editor.org/rfc/rfc9112.html#section-3.2.3) |
| H11-HOST-003 | MUST | Accept absolute-form even from direct clients (not only proxies). | [9112 §3.2.3](https://www.rfc-editor.org/rfc/rfc9112.html#section-3.2.3) |
| H11-HOST-004 | MUST | Reject CONNECT with empty/invalid port → **400**. | [9110 §9.3.6](https://www.rfc-editor.org/rfc/rfc9110.html#section-9.3.6) |
| H11-HOST-005 | MUST | Reject `https` scheme requirements if not received over valid TLS for that origin (unless trusted gateway). | [9110 §4.3.3](https://www.rfc-editor.org/rfc/rfc9110.html#section-4.3.3) |
| H11-HOST-006 | MAY | For empty authority on `http`/`https` URI: reject or apply configured default consistent with connection context. | [9112 §3.2.3](https://www.rfc-editor.org/rfc/rfc9112.html#section-3.2.3) |

---

## 3. Header section

### 3.1 Syntax and dangerous fields

| ID | Level | Invariant | RFC |
|----|-------|-----------|-----|
| H11-HDR-001 | MUST | Reject **400** any request with whitespace between header field name and `:`. | [9112 §5.2](https://www.rfc-editor.org/rfc/rfc9112.html#section-5.2) |
| H11-HDR-002 | MUST | On **obs-fold** in request (outside `message/http`): **400** (preferred) or replace fold with SP before interpreting. | [9112 §5.2](https://www.rfc-editor.org/rfc/rfc9112.html#section-5.2) |
| H11-HDR-003 | MUST | On `CR`, `LF`, or `NUL` in field value: **reject** message or replace with SP. | [9110 §5.5](https://www.rfc-editor.org/rfc/rfc9110.html#section-5.5) |
| H11-HDR-004 | MUST | Respond **4xx** when header line, value, or entire header section exceeds server limits. | [9110 §5.4](https://www.rfc-editor.org/rfc/rfc9110.html#section-5.4) |
| H11-HDR-005 | MUST NOT | Apply request until full header section received. | [9110 §5.3](https://www.rfc-editor.org/rfc/rfc9110.html#section-5.3) |
| H11-HDR-006 | SHOULD | Ignore unrecognized header/trailer fields (unless field definition says otherwise). | [9110 §5.1](https://www.rfc-editor.org/rfc/rfc9110.html#section-5.1) |

### 3.2 Response header obligations

| ID | Level | Invariant | RFC |
|----|-------|-----------|-----|
| H11-HDR-007 | MUST | Origin server with clock: generate **`Date`** on all **2xx, 3xx, 4xx** responses. | [9110 §6.6.1](https://www.rfc-editor.org/rfc/rfc9110.html#section-6.6.1) |
| H11-HDR-008 | MAY | Generate `Date` on 1xx and 5xx. | [9110 §6.6.1](https://www.rfc-editor.org/rfc/rfc9110.html#section-6.6.1) |
| H11-HDR-009 | MUST | Generate **`Allow`** on **405 (Method Not Allowed)**. | [9110 §10.2.1](https://www.rfc-editor.org/rfc/rfc9110.html#section-10.2.1) |
| H11-HDR-010 | MUST | Generate **`WWW-Authenticate`** on **401** (≥1 challenge). | [9110 §11.6.1](https://www.rfc-editor.org/rfc/rfc9110.html#section-11.6.1) |

---

## 4. Message body framing

Body length is determined by **[RFC 9112 §6.3](https://www.rfc-editor.org/rfc/rfc9112.html#section-6.3) precedence** (implement exactly this order):

1. `Transfer-Encoding: chunked` (final coding) → decode chunks until zero chunk + trailers.
2. `Transfer-Encoding` present in response, chunked not final → read until connection close.
3. `Transfer-Encoding` in request, chunked **not** final → **400**, close.
4. **HEAD** response, or **1xx / 204 / 304** → no body; terminated by empty line after headers.
5. **2xx CONNECT** → tunnel after header empty line; ignore TE/CL in that response.
6. **Both** `Transfer-Encoding` and `Content-Length` → TE wins; treat as error (smuggling); server **MUST close** after response.
7. Invalid `Content-Length` (unrecoverable) → **400**, close (request).
8. Valid `Content-Length` without TE → that many octets; incomplete → close.
9. Request, none of above → body length **0**.
10. Response, none of above → body until connection close.

| ID | Level | Invariant | RFC |
|----|-------|-----------|-----|
| H11-FRAME-001 | MUST | Implement §6.3 precedence **exactly** — no ad-hoc shortcuts. | [9112 §6.3](https://www.rfc-editor.org/rfc/rfc9112.html#section-6.3) |
| H11-FRAME-002 | MUST | On request TE present, chunked not final: **400**, close. | [9112 §6.3](https://www.rfc-editor.org/rfc/rfc9112.html#section-6.3) |
| H11-FRAME-003 | MUST | On invalid CL in request: **400**, close (unless comma-list duplicate same values). | [9112 §6.3](https://www.rfc-editor.org/rfc/rfc9112.html#section-6.3) |
| H11-FRAME-004 | MUST | On both TE and CL in request: process per TE **or** reject; **always close** after response. | [9112 §6.3](https://www.rfc-editor.org/rfc/rfc9112.html#section-6.3) |
| H11-FRAME-005 | MUST | On HTTP/1.0 message with `Transfer-Encoding`: treat framing faulty, close after processing. | [9112 §6.2](https://www.rfc-editor.org/rfc/rfc9112.html#section-6.2) |
| H11-FRAME-006 | MUST NOT | Send `Content-Length` in message that has `Transfer-Encoding`. | [9112 §6.2](https://www.rfc-editor.org/rfc/rfc9112.html#section-6.2) |
| H11-FRAME-007 | SHOULD | Prefer length-delimited or chunked responses over close-delimited. | [9112 §6.3](https://www.rfc-editor.org/rfc/rfc9112.html#section-6.3) |
| H11-FRAME-008 | MAY | Reject request body without `Content-Length` (non-chunked) with **411**. | [9112 §6.3](https://www.rfc-editor.org/rfc/rfc9112.html#section-6.3) |

### 4.1 Responses without body

| ID | Level | Invariant | RFC |
|----|-------|-----------|-----|
| H11-FRAME-009 | MUST NOT | Send `Content-Length` on **1xx** or **204** responses. | [9110 §8.6](https://www.rfc-editor.org/rfc/rfc9110.html#section-8.6), [9112 §6.1](https://www.rfc-editor.org/rfc/rfc9112.html#section-6.1) |
| H11-FRAME-010 | MUST NOT | Send `Transfer-Encoding` on **1xx** or **204** responses. | [9112 §6.1](https://www.rfc-editor.org/rfc/rfc9112.html#section-6.1) |
| H11-FRAME-011 | MUST NOT | Send `Content-Length` or `Transfer-Encoding` on **2xx CONNECT** response. | [9110 §9.3.6](https://www.rfc-editor.org/rfc/rfc9110.html#section-9.3.6), [9112 §6.1](https://www.rfc-editor.org/rfc/rfc9112.html#section-6.1) |
| H11-FRAME-012 | MUST NOT | Send content in **HEAD** response. | [9110 §9.3.2](https://www.rfc-editor.org/rfc/rfc9110.html#section-9.3.2) |
| H11-FRAME-013 | MUST NOT | Generate content in **205** response. | [9110 §15.3.6](https://www.rfc-editor.org/rfc/rfc9110.html#section-15.3.6) |

---

## 5. Chunked transfer coding

| ID | Level | Invariant | RFC |
|----|-------|-----------|-----|
| H11-CHUNK-001 | MUST | Be able to **parse and decode** chunked transfer coding. | [9112 §7.1](https://www.rfc-editor.org/rfc/rfc9112.html#section-7.1) |
| H11-CHUNK-002 | MUST | Ignore unrecognized **chunk extensions**. | [9112 §7.1.1](https://www.rfc-editor.org/rfc/rfc9112.html#section-7.1.1) |
| H11-CHUNK-003 | MUST | Anticipate large hex chunk-size values; prevent overflow/precision loss. | [9112 §7.1](https://www.rfc-editor.org/rfc/rfc9112.html#section-7.1) |
| H11-CHUNK-004 | SHOULD | Treat chunk extension parameters as error. | [9112 §7.1](https://www.rfc-editor.org/rfc/rfc9112.html#section-7.1) |
| H11-CHUNK-005 | ought | Limit total chunk-extension length; **4xx** if exceeded. | [9112 §7.1.1](https://www.rfc-editor.org/rfc/rfc9112.html#section-7.1.1) |
| H11-CHUNK-006 | MUST NOT | Send `Transfer-Encoding` on response unless request indicates HTTP/1.1+. | [9112 §6.1](https://www.rfc-editor.org/rfc/rfc9112.html#section-6.1) |
| H11-CHUNK-007 | SHOULD | Respond **501** to transfer coding not understood. | [9112 §6.1](https://www.rfc-editor.org/rfc/rfc9112.html#section-6.1) |

---

## 6. Connection management

### 6.1 Persistence defaults

| ID | Level | Invariant | RFC |
|----|-------|-----------|-----|
| H11-CONN-001 | SHOULD | Support **persistent connections** (default for HTTP/1.1). | [9112 §9.3](https://www.rfc-editor.org/rfc/rfc9112.html#section-9.3) |
| H11-CONN-002 | MUST | If persistence **not** supported: send `Connection: close` on every non-1xx response. | [9112 §9.3](https://www.rfc-editor.org/rfc/rfc9112.html#section-9.3) |
| H11-CONN-003 | MUST | On receiving `Connection: close` in request: close after that response; SHOULD echo `close`; MUST NOT process further requests. | [9112 §9.6](https://www.rfc-editor.org/rfc/rfc9112.html#section-9.6) |
| H11-CONN-004 | MUST | If sending `Connection: close`: close after that response; MUST NOT process further requests. | [9112 §9.6](https://www.rfc-editor.org/rfc/rfc9112.html#section-9.6) |
| H11-CONN-005 | SHOULD | Sustain persistent connections; prefer flow control over aggressive close. | [9112 §9.5](https://www.rfc-editor.org/rfc/rfc9112.html#section-9.5) |
| H11-CONN-006 | MAY | Reject abusive connection counts (DoS mitigation). | [9112 §9.4](https://www.rfc-editor.org/rfc/rfc9112.html#section-9.4) |

### 6.2 Pipelining

| ID | Level | Invariant | RFC |
|----|-------|-----------|-----|
| H11-PIPE-001 | MUST | Send pipelined responses in **same order** as requests received. | [9112 §9.3.2](https://www.rfc-editor.org/rfc/rfc9112.html#section-9.3.2) |
| H11-PIPE-002 | MAY | Process safe pipelined requests in parallel. | [9112 §9.3.2](https://www.rfc-editor.org/rfc/rfc9112.html#section-9.3.2) |

### 6.3 Incomplete messages

| ID | Level | Invariant | RFC |
|----|-------|-----------|-----|
| H11-INCOMPLETE-001 | MAY | Send error response before close on incomplete request (timeout/cancel). | [9112 §8](https://www.rfc-editor.org/rfc/rfc9112.html#section-8) |
| H11-INCOMPLETE-002 | MUST | Treat chunked body incomplete until zero-size chunk received. | [9112 §8](https://www.rfc-editor.org/rfc/rfc9112.html#section-8) |
| H11-INCOMPLETE-003 | MUST | Treat CL-delimited body incomplete if fewer octets received than CL. | [9112 §8](https://www.rfc-editor.org/rfc/rfc9112.html#section-8) |

---

## 7. HTTP semantics (origin server)

These apply once framing is resolved and the request is dispatched.

### 7.1 Methods (core)

| ID | Level | Invariant | RFC |
|----|-------|-----------|-----|
| H11-SEM-001 | MUST | **HEAD**: identical to GET but **no** response content. | [9110 §9.3.2](https://www.rfc-editor.org/rfc/rfc9110.html#section-9.3.2) |
| H11-SEM-002 | MUST | **PUT** creates resource → **201**; updates → **200** or **204**. | [9110 §9.3.4](https://www.rfc-editor.org/rfc/rfc9110.html#section-9.3.4) |
| H11-SEM-003 | MUST | **CONNECT** invalid port → **400**. | [9110 §9.3.6](https://www.rfc-editor.org/rfc/rfc9110.html#section-9.3.6) |
| H11-SEM-004 | MUST NOT | Assume two requests on same connection are same user agent (unless secured connection specific to agent). | [9110 §3.5](https://www.rfc-editor.org/rfc/rfc9110.html#section-3.5) |

### 7.2 Expect: 100-continue

| ID | Level | Invariant | RFC |
|----|-------|-----------|-----|
| H11-EXPECT-001 | MUST | On HTTP/1.1+ request with `Expect: 100-continue` and body forthcoming: send **100** or final **4xx** before reading body. | [9110 §10.1.1](https://www.rfc-editor.org/rfc/rfc9110.html#section-10.1.1) |
| H11-EXPECT-002 | MUST NOT | Wait for body before sending **100 Continue**. | [9110 §10.1.1](https://www.rfc-editor.org/rfc/rfc9110.html#section-10.1.1) |
| H11-EXPECT-003 | MUST | If `Upgrade` + `100-continue`: send **100** before **101**. | [9110 §7.8](https://www.rfc-editor.org/rfc/rfc9110.html#section-7.8) |

### 7.3 Upgrade / protocol switch

| ID | Level | Invariant | RFC |
|----|-------|-----------|-----|
| H11-UPG-001 | MUST | **101** response includes `Upgrade` listing new protocol(s), layer-ascending. | [9110 §7.8](https://www.rfc-editor.org/rfc/rfc9110.html#section-7.8), [9110 §15.2.2](https://www.rfc-editor.org/rfc/rfc9110.html#section-15.2.2) |
| H11-UPG-002 | MUST NOT | Switch to protocol not indicated in client `Upgrade`. | [9110 §7.8](https://www.rfc-editor.org/rfc/rfc9110.html#section-7.8) |
| H11-UPG-003 | MUST | **426** includes `Upgrade` with acceptable protocols. | [9110 §7.8](https://www.rfc-editor.org/rfc/rfc9110.html#section-7.8) |
| H11-UPG-004 | MUST NOT | Switch unless new protocol can honor received message semantics. | [9110 §7.8](https://www.rfc-editor.org/rfc/rfc9110.html#section-7.8) |

### 7.4 Conditional requests (when supported)

| ID | Level | Invariant | RFC |
|----|-------|-----------|-----|
| H11-COND-001 | MUST | Evaluate `If-Match` before method (strong comparison). | [9110 §13.1](https://www.rfc-editor.org/rfc/rfc9110.html#section-13.1) |
| H11-COND-002 | MUST | Evaluate `If-None-Match`; false → **304** (GET/HEAD) or **412** (others). | [9110 §13.1](https://www.rfc-editor.org/rfc/rfc9110.html#section-13.1) |
| H11-COND-003 | MUST | Evaluate preconditions in order defined §13.2. | [9110 §13.2](https://www.rfc-editor.org/rfc/rfc9110.html#section-13.2) |
| H11-COND-004 | MUST | Ignore `If-Range` when no `Range` header. | [9110 §13.1.5](https://www.rfc-editor.org/rfc/rfc9110.html#section-13.1.5) |
| H11-COND-005 | MUST | Ignore `Range` for methods other than GET (when not defined). | [9110 §14.1](https://www.rfc-editor.org/rfc/rfc9110.html#section-14.1) |

### 7.5 Range requests (when supported)

| ID | Level | Invariant | RFC |
|----|-------|-----------|-----|
| H11-RANGE-001 | MUST | **206** with multiple parts: `multipart/byteranges` + per-part `Content-Range`. | [9110 §15.3.7](https://www.rfc-editor.org/rfc/rfc9110.html#section-15.3.7) |
| H11-RANGE-002 | MUST NOT | `Content-Range` in header of multipart 206 (only in parts). | [9110 §15.3.7](https://www.rfc-editor.org/rfc/rfc9110.html#section-15.3.7) |

### 7.6 Validators (when sending cache validators)

| ID | Level | Invariant | RFC |
|----|-------|-----------|-----|
| H11-VAL-001 | MUST NOT | `Last-Modified` later than message `Date`. | [9110 §8.8.2](https://www.rfc-editor.org/rfc/rfc9110.html#section-8.8.2) |
| H11-VAL-002 | MUST | Prefix weak entity tags with `W/` when not strong validators. | [9110 §8.8.3](https://www.rfc-editor.org/rfc/rfc9110.html#section-8.8.3) |

---

## 8. Security invariants

Derived from [RFC 9112 §11](https://www.rfc-editor.org/rfc/rfc9112.html#section-11). **Violations are correctness bugs, not hardening extras.**

| ID | Level | Invariant | RFC |
|----|-------|-----------|-----|
| H11-SEC-001 | MUST | Single consistent parsing algorithm — no ambiguous framing between requests on one connection. | [9112 §11.2](https://www.rfc-editor.org/rfc/rfc9112.html#section-11.2) |
| H11-SEC-002 | MUST | Prevent **response splitting**: no CR/LF in header fields you generate; sanitize application output to header APIs. | [9112 §11.1](https://www.rfc-editor.org/rfc/rfc9112.html#section-11.1) |
| H11-SEC-003 | MUST | Defeat **request smuggling**: strict §6.3 framing, reject CL+TE, reject bad CL, reject ambiguous TE. | [9112 §6.3](https://www.rfc-editor.org/rfc/rfc9112.html#section-6.3), [9112 §11.2](https://www.rfc-editor.org/rfc/rfc9112.html#section-11.2) |
| H11-SEC-004 | MUST | Reject colon whitespace in header names (**400**). | [9112 §5.2](https://www.rfc-editor.org/rfc/rfc9112.html#section-5.2) |
| H11-SEC-005 | MUST NOT | Ignore oversize headers (increases smuggling surface). | [9110 §5.4](https://www.rfc-editor.org/rfc/rfc9110.html#section-5.4) |

---

## 9. TLS (HTTP over TLS)

When serving `https`:

| ID | Level | Invariant | RFC |
|----|-------|-----------|-----|
| H11-TLS-001 | MUST | All HTTP data sent as TLS application data. | [9112 §9.7](https://www.rfc-editor.org/rfc/rfc9112.html#section-9.7) |
| H11-TLS-002 | MUST | Attempt TLS closure alert exchange before closing. | [9112 §9.8](https://www.rfc-editor.org/rfc/rfc9112.html#section-9.8) |
| H11-TLS-003 | SHOULD | Handle client incomplete close; use HTTP framing to detect complete messages. | [9112 §9.8](https://www.rfc-editor.org/rfc/rfc9112.html#section-9.8) |
| H11-TLS-004 | MUST | Reject `https` semantics on cleartext connection (unless trusted gateway). | [9110 §4.3.3](https://www.rfc-editor.org/rfc/rfc9110.html#section-4.3.3) |

---

## 10. Parser state machine invariants

These are **implementation contracts** for KinetiC’s incremental parser (to be codified in code). They restate RFC rules in stateful form.

| State | Enter when | Invariant before leaving | RFC basis |
|-------|------------|--------------------------|-----------|
| `IDLE` | Connection open | Buffer may contain CRLF padding only (optional skip) | §2.2 |
| `REQUEST_LINE` | Start of request | Method, target, version parsed or error | §3 |
| `REQUEST_HEADERS` | After request CRLF | All fields parsed; `Host` validated for HTTP/1.1; framing decision possible | §5, §6.3, §7.2 |
| `REQUEST_BODY` | Framing > 0 | Exactly N octets consumed (CL, chunked, or EOF per rules) | §6.3 |
| `DISPATCH` | Body done or absent | No application handler until headers complete (and body if required) | §5.3 |
| `RESPONSE` | Handler ready | Status-line + headers + body obey framing rules for status/method | §4, §6 |
| `COMPLETE` | Response fully sent | If persistent: idle with no unread request body remainder | §9.3 |

**Transition rules**

| ID | Invariant |
|----|-----------|
| H11-FSM-001 | `REQUEST_HEADERS` → `DISPATCH` without `Host` check on HTTP/1.1 is forbidden. |
| H11-FSM-002 | `REQUEST_BODY` → `DISPATCH` with partial body on persistent connection is forbidden. |
| H11-FSM-003 | `RESPONSE` → `IDLE` requires response message complete per §6.3 for that status/method. |
| H11-FSM-004 | Any unrecoverable framing error → emit error response if possible, then `CLOSING`. |
| H11-FSM-005 | After `CLOSING` initiated for smuggling/framing error, do not read another request on same connection. |

---

## 11. Explicit non-goals (this document)

The following are **out of scope** for HTTP/1.1 wire compliance unless a later doc says otherwise:

- HTTP/2, HTTP/3 ([RFC 9113](https://www.rfc-editor.org/rfc/rfc9113.html), [RFC 9114](https://www.rfc-editor.org/rfc/rfc9114.html))
- Full HTTP caching & proxy cache store behavior (see [CACHING.md](CACHING.md) / [RFC 9111](https://www.rfc-editor.org/rfc/rfc9111.html))
- WebSocket handshake (uses Upgrade but is defined elsewhere)
- Optional extensions: trailers, compression transfer codings beyond `chunked`, upgrade to non-HTTP protocols

---

## 12. Planned topic documents

Each section above will become a dedicated design note. Suggested filenames:

| Topic | File (planned) |
|-------|----------------|
| Lifecycle & connection | `lifecycle.md` |
| Request-line & request-target | `request-target.md` |
| Host validation | `host.md` |
| Header parsing | `headers.md` |
| Body framing & precedence | `framing.md` |
| Chunked encoding | `chunked.md` |
| Persistence, pipelining, close | `connections.md` |
| Security (smuggling/splitting) | `security.md` |
| Methods & status codes | `semantics.md` |
| Expect / 100-continue | `expect-continue.md` |
| Parser state machine | `parser-fsm.md` |
| TLS mapping | `tls.md` |

---

## 13. Compliance stance for KinetiC

1. **MUST** items are tracked as hard requirements in parser, connection, and handler code.
2. **SHOULD** items are default behavior; deviations require a comment citing why.
3. Fuzzing and regression tests target **H11-SEC-*** and **H11-FRAME-*** first.
4. When implementing the parser FSM, every transition documents which invariant IDs it satisfies.

*Last updated from RFC 9110 / RFC 9112 (June 2022). Re-verify when IETF publishes errata.*
