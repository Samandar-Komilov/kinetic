# KinetiC tutorials — index

*Osman builds an HTTP/1.1 server one RFC layer at a time. Each tutorial is a step you follow and edit along the way — the code is yours, the spec is the teacher.*

## What exists in the repo today

Nothing on the wire yet. Deliberately minimal:

| Piece | Location | Purpose |
|-------|----------|---------|
| `ktc_str` | `include/ktc/core/str.h` | Non-owning octet slice `{ptr, len}` for wire data |
| `ktc_arena` | `include/ktc/core/arena.h` | Bump allocator; per-connection/request lifetime later |
| `kinetic` binary | `src/main.c` | Prints version; **your** libuv/YAML/HTTP code grows here |
| Invariants checklist | [INVARIANTS.md](../http1.1/INVARIANTS.md) | Full RFC 9110/9112 obligations (reference) |

**Not in the repo (you build these):** YAML loader, TCP listener, HTTP parser, routing config. Premature abstractions stay out so every line is yours.

**Naming:** public API prefix is `ktc_`. Project name stays **KinetiC**.

## The arc

```text
T0  Foundation   Eras 0–3   → first lawful HTTP/1.1 response (curl gets 200)
T1  Bodies       Era 4      → message body framing (Content-Length, chunked)
T2  Persistence  Era 5      → persistent connections / Connection: close
T3  Pipelining   Era 6      → pipelining, response ordering
T4  Methods      Era 7      → HTTP methods (GET, HEAD, OPTIONS)
T5  Expect       Era 8      → Expect: 100-continue header checks
T6  Security     Era 9      → security hardening & desync protections
T7  Production   Era 10+    → caching, gzip compression, TLS, workers
```

Each tutorial (except this index) is an **Osman story**: scene, 2–3 RFC sections, invariant IDs, FSM sketch, exit criteria.

## Osman's rule

> Do not skip eras. Do not import an HTTP parser library. The spec is the teacher; `ktc_str` and `ktc_arena` are the only shipped helpers until a tutorial says otherwise.

## Reading order

1. **[T0 — Foundation](T0_foundation.md)** — Eras 0–3: connection, request-line, headers, first `200`
2. **[T1 — Bodies](T1_bodies.md)** — Era 4: body framing
3. **[T2 — Persistence](T2_persistence.md)** — Era 5: keep-alive
4. **[T3 — Pipelining](T3_pipelining.md)** — Era 6: ordered responses on one connection
5. **[T4 — Methods & Semantics](T4_methods.md)** — Era 7: HTTP methods (GET, HEAD, OPTIONS)
6. **[T5 — Expect: 100-continue](T5_expect.md)** — Era 8: Expect header validation
7. **[T6 — Security Hardening](T6_security.md)** — Era 9: request smuggling & overflow guards
8. **[T7 — Production Readiness](T7_prod_negotiation.md)** — Era 10+: caching, gzip compression, TLS, workers

Start with T0. Tutorials T1–T7 continue **after** Era 3 is green.

---

*When T0 Era 3 passes `curl -v http://127.0.0.1:8080/` with a valid `HTTP/1.1 200`, Osman has a real HTTP/1.1 front half. Tutorials T1–T3 take Osman toward a production-grade wire implementation.*
