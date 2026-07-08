# T3 — Pipelining: Osman serves a queue

*Era 6. The connection stays open. The client stops waiting — pipelining begins.*

Continue from [Tutorial T2 — Era 5](T2_persistence.md). Index: [README](README.md).

---

## Story

Osman's server handles keep-alive. A impatient client sends:

```http
GET /a HTTP/1.1\r\n
Host: localhost\r\n
\r\n
GET /b HTTP/1.1\r\n
Host: localhost\r\n
\r\n
```

…before reading either response. Two complete requests sit in the read buffer. Osman **must** parse them in order and **must** send **two** responses in the **same order** — even if `/b` would be faster to compute.

Mis-associating response to request breaks caching, cookies, and every HTTP client library. Era 6 is about **ordering**, not throughput bragging.

---

## RFC grounding (2–3 sections)

| RFC | Section | What Osman must implement |
|-----|---------|-------------------------|
| [RFC 9112](https://www.rfc-editor.org/rfc/rfc9112.html) | **§9.3.2** | Pipelined responses **MUST** match request order; parallel processing only for **safe** methods |
| [RFC 9112](https://www.rfc-editor.org/rfc/rfc9112.html) | **§9.2** | Associate each response with the first outstanding request without a final response |
| [RFC 9110](https://www.rfc-editor.org/rfc/rfc9110.html) | **§9.2.1** | Safe methods (`GET`, `HEAD`, …) vs idempotent — informs whether parallel work is allowed |

---

## Invariants (this era)

| ID | Rule |
|----|------|
| H11-PIPE-001 | Send pipelined responses in **same order** as requests received |
| H11-PIPE-002 | **MAY** process safe pipelined requests in parallel |
| H11-LIFE-001 | Ordered request/response pairs on one connection |

---

## Architecture sketch

```text
read buffer may contain:  [req1][req2 partial...]

parse loop:
  while enough bytes for full message N:
    enqueue request N (or handle inline)
    if not pipelined mode: stop after 1

response queue:
  responses MUST depart in enqueue order
  (even if handler for req2 finished before req1)
```

**Pragmatic Era 6 for Osman:**

1. **Phase A:** Parse requests sequentially from buffer; respond before reading next (no true pipeline) — validates parser reset between messages.
2. **Phase B:** Allow multiple complete requests buffered; maintain FIFO response queue.
3. **Phase C (optional):** Parallel `GET` handlers with ordered write queue.

Parser after each complete **request** (headers + body):

```text
MESSAGE_COMPLETE → DISPATCH → RESPONSE_QUEUED → (when channel free) WRITE
  → if more bytes in buffer: IDLE → REQUEST_LINE (same connection)
  → else: wait for READ
```

`ktc_arena_reset` between requests on same connection.

---

## What can go wrong

| Failure | Symptom |
|---------|---------|
| Response B sent before A | Client mis-parses; protocol violation |
| Parser not reset between messages | Second request-line starts mid-header |
| Body not drained before next parse | Smuggling / desync ([H11-SEC-003](../http1.1/INVARIANTS.md)) |

---

## What Osman gains after Era 6

Osman is not production-complete — but the **wire core** is recognizable:

- Octet-accurate parsing through body
- Keep-alive
- Ordered pipelining for safe methods

Era 7+ (future tutorials): method semantics (`HEAD`, `OPTIONS`, `POST`), `Expect: 100-continue`, full security audit, TLS, master/worker.

---

## Exit criteria

- [ ] Client sends two pipelined `GET`s; receives two **200**s in order (verify with trace or `curl --http1.1` pipeline).
- [ ] Second request parsed only after first message fully consumed (including body if any).
- [ ] Parser FSM resets cleanly between messages on persistent connection.
- [ ] No response reordering under parallel handler completion (if Phase C attempted).
- [ ] Era 2–5 regression tests pass.

**Osman's diary:** *They talk faster than I answer. I still reply in the order they spoke.*

---

## Epilogue — toward prod

Osman's evolution from [T0](T0_foundation.md) Era 0 through Era 6 covers the **HTTP/1.1 messaging core**. What remains for a prod-ready server (each deserves its own tutorial era):

| Era | Topic | RFC anchor |
|-----|-------|------------|
| 7 | Methods (`HEAD`, `OPTIONS`, `POST`) | [9110 §9](https://www.rfc-editor.org/rfc/rfc9110.html#section-9) |
| 8 | `Expect: 100-continue` | [9110 §10.1.1](https://www.rfc-editor.org/rfc/rfc9110.html#section-10.1.1) |
| 9 | Security hardening (smuggling, splitting) | [9112 §11](https://www.rfc-editor.org/rfc/rfc9112.html#section-11) |
| 10+ | Routing config, workers, TLS | project roadmap |

Full obligation list: [INVARIANTS.md](../http1.1/INVARIANTS.md).

*The spec is long. Osman is no longer afraid of it.*
