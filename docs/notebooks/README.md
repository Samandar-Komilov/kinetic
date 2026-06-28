# KinetiC notebooks — index

*Sam's server evolves one RFC layer at a time. You write the code; the notebooks are the story.*

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
notebook0   Eras 0–3   → first lawful HTTP/1.1 response (curl gets 200)
notebook1   Era 4      → message body framing (Content-Length, chunked)
notebook2   Era 5      → persistent connections / Connection: close
notebook3   Era 6      → pipelining, response ordering
…           Era 7+     → methods, Expect, security deep-dive (future)
```

Each notebook (except this index) is a **Sam story**: scene, 2–3 RFC sections, invariant IDs, FSM sketch, exit criteria.

## Sam's rule

> Do not skip eras. Do not import an HTTP parser library. The spec is the teacher; `ktc_str` and `ktc_arena` are the only shipped helpers until a notebook says otherwise.

## Reading order

1. **[Notebook 0 — Foundation](notebook0.md)** — Eras 0–3: connection, request-line, headers, first `200`
2. **[Notebook 1 — Bodies](notebook1.md)** — Era 4: body framing
3. **[Notebook 2 — Persistence](notebook2.md)** — Era 5: keep-alive
4. **[Notebook 3 — Pipelining](notebook3.md)** — Era 6: ordered responses on one connection

Start with notebook 0. Notebooks 1–3 continue **after** Era 3 is green.

---

*When notebook 0 Era 3 passes `curl -v http://127.0.0.1:8080/` with a valid `HTTP/1.1 200`, Sam has a real HTTP/1.1 front half. Notebooks 1–3 take Sam toward a production-grade wire implementation.*
