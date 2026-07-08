# T2 — Persistence: Osman keeps the door open

*Era 5. Every response carried `Connection: close`. Osman learns HTTP/1.1's default: the connection lives.*

Continue from [Tutorial T1 — Era 4](T1_bodies.md). Index: [README](README.md).

---

## Story

Osman's server handles bodies correctly — but tears down TCP after each reply. Browsers and `curl` open a new connection for every request. That works; it is not how HTTP/1.1 is meant to run at scale.

A client sends two requests on **one** connection:

```http
GET /a HTTP/1.1\r\n
Host: localhost\r\n
\r\n
GET /b HTTP/1.1\r\n
Host: localhost\r\n
\r\n
```

If Osman closes after the first response, the second never arrives on a fresh socket — fine. If Osman **keeps** the connection open but fails to **read the entire first request body** (tutorial T1), leftover bytes become a corrupted second request-line — one of the oldest HTTP server bugs.

Era 5 is about **persistence**: when to keep the connection, when to close, and how to shut down without RST-ing the client.

---

## RFC grounding (2–3 sections)

| RFC | Section | What Osman must implement |
|-----|---------|-------------------------|
| [RFC 9112](https://www.rfc-editor.org/rfc/rfc9112.html) | **§9.3** | Persistent connections default for HTTP/1.1; **must read entire request body** or close |
| [RFC 9112](https://www.rfc-editor.org/rfc/rfc9112.html) | **§9.6** | `Connection: close`; staged tear-down (half-close write, drain read) |
| [RFC 9112](https://www.rfc-editor.org/rfc/rfc9112.html) | **§8** | Incomplete messages — timeouts, partial chunked body |

---

## Invariants (this era)

| ID | Rule |
|----|------|
| H11-LIFE-003 | After response, read **entire** request body or close connection |
| H11-LIFE-004 | Every message on persistent conn needs **self-defined length** |
| H11-LIFE-005 | After honoring `Connection: close`, process **no** further requests |
| H11-LIFE-006 | **SHOULD** close in stages — avoid TCP RST wiping final response |
| H11-CONN-001 | **SHOULD** support persistent connections |
| H11-CONN-003–004 | Handle client/server `Connection: close` correctly |
| H11-INCOMPLETE-002–003 | Incomplete chunked or CL body → treat as incomplete |

---

## Connection policy

**Default (HTTP/1.1 request):**

- Omit `Connection: close` on response → connection stays open.
- After response fully sent, return parser to **IDLE** for next request-line on same socket.
- Reset `ktc_arena` per request (`ktc_arena_reset`) — not destroy — on keep-alive.

**When to close:**

- Client sends `Connection: close`.
- Osman sends `Connection: close` (error paths, HTTP/1.0 client, unsupported persistence).
- Framing error ([T1](T1_bodies.md) smuggling cases).
- Idle timeout (implementation choice; RFC does not mandate length).

```text
RESPONSE_SENT
  → if close_requested: CLOSING (staged)
  → else: IDLE (next request-line on same conn)
```

---

## Graceful shutdown ([9112 §9.6](https://www.rfc-editor.org/rfc/rfc9112.html#section-9.6))

Rough sequence:

1. Finish writing response.
2. `uv_shutdown` write side (half-close).
3. Continue reading until client closes or timeout.
4. `uv_close` handle.

Skipping drain risks client seeing RST instead of full response body.

---

## What Osman must not do yet

- **Pipelining** — multiple requests in flight before responses ([T3](T3_pipelining.md)). For Era 5, read one full request, send one response, then read the next.
- Parallel handler threads on one connection.

---

## Exit criteria

- [ ] Two sequential `GET`s on one connection → two **200** responses; no byte corruption.
- [ ] `POST` with body on persistent conn — body fully drained before next request-line parsed.
- [ ] Client `Connection: close` → Osman closes after that response; no further reads treated as new requests.
- [ ] Osman `Connection: close` on error → connection ends.
- [ ] 10 connect/request/response cycles without handle leak.
- [ ] Regression: Era 3–4 tests still pass.

**Osman's diary:** *The door stays open. I finish reading everything the client owed me before I listen for the next sentence.*

---

## Next

[Tutorial T3 — Pipelining](T3_pipelining.md): client sends request B before response A — Osman must not reorder replies.
