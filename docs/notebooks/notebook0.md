# Notebook 0 — The Evolution of Sam's Server

*A story in three RFC milestones through Era 3. Read [INVARIANTS.md](../http1.1/INVARIANTS.md) for the full checklist; this notebook is the path from nothing to the first lawful HTTP response.*

See also: [notebooks index](README.md) for reading order and follow-up notebooks 1–3 (Eras 4–6).

---

## Prologue: Sam and the empty machine

Sam is building a server. Not another framework — a **server**: something that sits on a port, reads bytes from the network, and speaks HTTP/1.1 by the book ([RFC 9112](https://www.rfc-editor.org/rfc/rfc9112.html), [RFC 9110](https://www.rfc-editor.org/rfc/rfc9110.html)).

Right now Sam's server does almost nothing useful on the wire. The repo ships only `ktc_str` and `ktc_arena`; `main` prints a version string. That is **Era 0**: helpers and tooling, but no listener, no client, no protocol — **you** write every layer as you read this notebook.

The evolution ahead is not a feature dump. It is **layers of obligation** — each era adds RFC rules Sam must satisfy before the next era is even meaningful. The main actor is always **Sam's server**. The client is whoever knocks on the port. The spec is the referee.

```text
Era 0   today     ktc_str + ktc_arena + minimal main
Era 1   ───────►  a connection exists; bytes flow in order
Era 2   ───────►  the first line is a legal request-line
Era 3   ───────►  headers complete; Host is judged; a response leaves the wire
```

Everything else — bodies, persistence, pipelining, methods — comes in [notebook 1](notebook1.md) onward. Sam does not skip eras.

### Your practical build order (inside this notebook)

These steps are **Eras 0–3**, not separate numbered notebooks:

1. **Explore libuv** — loops, handles, `uv_tcp_t`, read callbacks (scratch programs OK).
2. **Minimal YAML** — parse only `kinetic.name` and `listen_port` from [configs/kinetic.yaml.example](../../configs/kinetic.yaml.example).
3. **Listen and read octets** — bind port, append bytes to a buffer, view as `ktc_str`.
4. **Request-line parser** — incremental FSM through first CRLF.
5. **Headers + Host + response** — full header section, then `HTTP/1.1 200`.

---

## Era 1 — Sam learns to hold a connection

> *"Before HTTP, there is a reliable stream of octets in order."*  
> — [RFC 9112 §9](https://www.rfc-editor.org/rfc/rfc9112.html#section-9)

### The scene

Sam binds a TCP port (from config, once YAML lands in step 2). A client connects. For the first time, Sam's server is not a program that runs and stops — it is a **process that waits**.

Nothing is parsed yet. Sam might not even know these bytes are HTTP. That is fine. Era 1 is not about grammar; it is about **connection management**, the substrate every later rule assumes.

### What the RFC demands (this era)

| ID | Rule (short) |
|----|----------------|
| H11-LIFE-001 | One connection, ordered messages — responses will eventually match request order. |
| — | HTTP presumes a **reliable, in-order** transport ([9112 §9](https://www.rfc-editor.org/rfc/rfc9112.html#section-9)). |
| — | Sam **associates** each future response with the request that caused it ([9112 §9.2](https://www.rfc-editor.org/rfc/rfc9112.html#section-9.2)). |

Sam is allowed to close a connection at any time ([9112 §9.5](https://www.rfc-editor.org/rfc/rfc9112.html#section-9.5)). For now, every response ends with `Connection: close` — persistence is [notebook 2](notebook2.md).

### Core types that land here

| Type | Use in Era 1 |
|------|----------------|
| `ktc_str` | View received bytes as `{ptr, len}` octets — never `strlen` on wire data |
| `ktc_arena` | Optional: allocate per-connection state; destroy on close |

When libuv delivers `uv_buf_t`, adapt to `ktc_str` in **your** code (add `ktc_str_from_uv_buf` when you need it). Slices must not outlive the read buffer.

### What Sam's server can do after Era 1

- Listen on `listen_port` (libuv `uv_tcp_t`).
- Accept connections; one handle per client.
- Read incoming bytes into a buffer (no interpretation).
- On client hang-up: free handles, return to listening.
- Optionally echo raw bytes back (debug only — **not** HTTP yet).

### What Sam must not pretend yet

- No request-line parsing.
- No status codes.
- No claim of "HTTP server" in the compliance sense.

### Implementation sketch

```text
states: LISTENING | CONNECTED | CLOSING
events: ACCEPT | READ | EOF | ERROR | CLOSE
```

- **LISTENING** → `ACCEPT` → **CONNECTED**
- **CONNECTED** → `EOF` | `CLOSE` → **CLOSING** → back to **LISTENING**

Suggested layout (you create):

```text
src/net/listener.c
src/net/connection.c
```

### Era 1 exit criteria

- [ ] `curl` or `nc` can open TCP to Sam's port (even if Sam only logs bytes).
- [ ] Multiple sequential clients work; no handle leaks.
- [ ] Bytes read are stored as **octets**, exposed via `ktc_str` (`H11-PARSE-001` mindset).

**Sam's diary:** *I have a door. People can knock. I can hear them. I don't understand them yet.*

---

## Era 2 — Sam learns the request-line

> *"The start-line tells you what kind of message this is."*  
> — [RFC 9112 §3](https://www.rfc-editor.org/rfc/rfc9112.html#section-3)

### The scene

A client sends:

```http
GET /hello HTTP/1.1\r\n
```

Sam's buffer fills. Era 2 begins when Sam stops treating the stream as opaque and starts hunting for the **first CRLF**: the end of the **request-line**.

Grammar ([9112 §3](https://www.rfc-editor.org/rfc/rfc9112.html#section-3)):

```text
request-line = method SP request-target SP HTTP-version CRLF
```

Sam extracts three tokens and a version. Until this line is valid, nothing behind it is trustworthy.

### What the RFC demands (this era)

| ID | Rule (short) |
|----|----------------|
| H11-PARSE-001 | Parse as **octets** (US-ASCII superset), not Unicode. |
| H11-PARSE-006 | **SHOULD** ignore one or more empty lines before the request-line. |
| H11-PARSE-007 | On grammar violation: **SHOULD** **400** and close. |
| H11-REQLINE-001 | Recognize request-target forms (origin-form first; others stubbed). |
| H11-REQLINE-002 | **414** if request-target exceeds limit. |
| H11-REQLINE-003 | **501** if method token exceeds implemented set. |
| H11-REQLINE-004 | **SHOULD NOT** silently fix a broken request-line. |
| H11-REQLINE-005 | **Strict** whitespace — no lenient parsing yet (smuggling risk). |

### Parser state machine (Era 2 only)

```text
IDLE ──► SKIP_EMPTY_LINES? ──► REQUEST_LINE ──► (Era 3: HEADERS)
                │                    │
                │                    └── bad grammar → 400, CLOSING
                └── optional CRLF CRLF before line
```

### What Sam's server can do after Era 2

- Incrementally parse the request-line from a byte buffer (partial reads OK).
- Know `method`, `request-target`, `HTTP-version` before reading headers.
- Reply with **400** on bad line (minimal status-line skeleton OK for practice).

Token extraction uses `ktc_str` slices into the receive buffer — no copy unless you choose to.

### What Sam defers

- Header fields (`Host`, `Content-Length`, …).
- Message body.
- `GET` semantics beyond parsing (no resource yet).

### Era 2 exit criteria

- [ ] Valid `GET /path HTTP/1.1` parsed under split TCP segments.
- [ ] Leading blank line ignored (`H11-PARSE-006`).
- [ ] Malformed line → **400** or close per table above.
- [ ] Parser is incremental — no "read entire socket and sscanf."

Suggested layout:

```text
src/http/req_line.c
include/ktc/http/req_line.h
```

**Sam's diary:** *I understand the first sentence clients speak. Method, path, version. If the sentence is nonsense, I say so and hang up.*

---

## Era 3 — Sam learns headers, Host, and the first lawful response

> *"Do not touch the resource until the full header section has arrived."*  
> — [RFC 9110 §5.3](https://www.rfc-editor.org/rfc/rfc9110.html#section-5.3)

### The scene

The request-line was valid. More lines follow:

```http
Host: localhost:8080\r\n
User-Agent: curl/8.x\r\n
\r\n
```

The empty line (`CRLF`) ends the **header section**. Only now — **not one byte earlier** — may Sam decide what this request *means*.

This is the first era where Sam is recognizably an **HTTP/1.1 server**, because Sam enforces **`Host`**.

### What the RFC demands (this era)

**Header syntax**

| ID | Rule (short) |
|----|----------------|
| H11-HDR-001 | Whitespace before `:` in field name → **400**. |
| H11-HDR-002 | Obsolete line folding (obs-fold) → **400** or replace with SP. |
| H11-HDR-003 | `CR` / `LF` / `NUL` in field values → reject or sanitize. |
| H11-HDR-004 | Oversized headers → **4xx**. |
| H11-LIFE-002 | **MUST NOT** dispatch until entire header section received. |

**Host** ([9110 §7.2](https://www.rfc-editor.org/rfc/rfc9110.html#section-7.2), [9112 §3.2](https://www.rfc-editor.org/rfc/rfc9112.html#section-3.2))

| ID | Rule (short) |
|----|----------------|
| H11-HOST-001 | HTTP/1.1 without valid singular `Host` → **400**. |
| H11-HOST-002 | Absolute-form target: authority from URI, ignore `Host` header. |
| H11-HOST-003 | Must accept absolute-form (even from direct clients). |

**Responses Sam sends** ([9112 §4](https://www.rfc-editor.org/rfc/rfc9112.html#section-4), [9110 §6.2](https://www.rfc-editor.org/rfc/rfc9110.html#section-6.2))

| ID | Rule (short) |
|----|----------------|
| H11-STATUS-001 | Status-line: SP required after status-code. |
| H11-STATUS-002 | Response version must be one Sam implements. |
| H11-STATUS-003 | **SHOULD** mirror request major version (≤ client's). |
| H11-HDR-007 | Origin server with clock: **`Date`** on 2xx/3xx/4xx. |

**Framing for this era only**

Assume **no request body**: `GET`/`HEAD` without `Content-Length` or `Transfer-Encoding`. Body length = 0 ([9112 §6.3](https://www.rfc-editor.org/rfc/rfc9112.html#section-6.3) rule 6).

Response example:

```http
HTTP/1.1 200 OK\r\n
Date: Sat, 26 Jun 2026 12:00:00 GMT\r\n
Content-Length: 0\r\n
Connection: close\r\n
\r\n
```

Sam **closes** after each response in Era 3 — persistence is [notebook 2](notebook2.md).

Use `ktc_str_eq_ci` for header **name** comparison (`Host` vs `host`).

### Parser state machine (Eras 2–3)

```text
IDLE
  → REQUEST_LINE
  → REQUEST_HEADERS   (field-line loop until empty line)
  → VALIDATE_HOST     (HTTP/1.1 only)
  → DISPATCH_MINIMAL  (static 200 or 405)
  → WRITE_RESPONSE
  → CLOSING
```

Invariant gates:

- `H11-FSM-001`: never `DISPATCH` without `Host` check on HTTP/1.1.
- `H11-FSM-004`: unrecoverable parse error → error response if possible, then `CLOSING`.

### What Sam's server can do after Era 3

```bash
curl -v http://127.0.0.1:8080/
```

- Parse request-line + headers incrementally.
- Reject missing/duplicate `Host` with **400**.
- Return **200** (or **405** for unsupported methods) with valid status-line + `Date` + `Content-Length` + `Connection: close`.
- Close cleanly.

### What Sam still defers (follow-up notebooks)

| Next | Topic | Notebook |
|------|-------|----------|
| Era 4 | Request/response **body framing** | [notebook 1](notebook1.md) |
| Era 5 | **Persistent** connections | [notebook 2](notebook2.md) |
| Era 6 | **Pipelining** order | [notebook 3](notebook3.md) |
| Era 7+ | Methods, Expect, caching, TLS | future |

### Era 3 exit criteria

- [ ] `curl http://host:port/` → **200**, valid `Date`, connection closes.
- [ ] Request without `Host` → **400**.
- [ ] Two `Host` headers → **400**.
- [ ] Header parser survives split packets; strict CRLF on generation.

Suggested layout:

```text
src/http/headers.c
src/http/response.c
```

**Sam's diary:** *I wait for the full header block. I judge Host. I speak back with a proper status-line and Date. For the first time, curl prints HTTP/1.1 200. I am a server.*

---

## Epilogue: three eras, one arc — and what comes next

These three eras map to the **first vertical slice** of the spec:

| Era | RFC chapter | Sam gains |
|-----|-------------|-----------|
| **1** | [9112 §9](https://www.rfc-editor.org/rfc/rfc9112.html#section-9) Connection management | A living connection |
| **2** | [9112 §2–3](https://www.rfc-editor.org/rfc/rfc9112.html#section-2) Message grammar + request-line | Structured input |
| **3** | [9112 §4–5](https://www.rfc-editor.org/rfc/rfc9112.html#section-4) + [9110 §5.3, §7.2, §6.6.1](https://www.rfc-editor.org/rfc/rfc9110.html#section-5.3) | Headers, Host, lawful response |

That is the story **from zero to first conversation**.

When Era 3's checklist is green, continue Sam's journey:

1. **[Notebook 1 — Bodies](notebook1.md)** — Era 4: `Content-Length`, chunked, §6.3 precedence
2. **[Notebook 2 — Persistence](notebook2.md)** — Era 5: keep-alive, drain bodies, graceful close
3. **[Notebook 3 — Pipelining](notebook3.md)** — Era 6: ordered responses on one connection

*Yahoo. Here we go.*
