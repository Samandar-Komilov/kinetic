# KinetiC — Agent Guide

> MANDATORY: Do not commit! The author reviews and then commits always!

## What this project is

**KinetiC** — HTTP/1.1 server in C, built by the author following `docs/tutorials/`. The repo intentionally stays thin: only `ktc_str` and `ktc_arena` helpers exist. **Do not** add config parsers, net modules, or HTTP parsers unless the user asks for a specific tutorial era.

## Non-negotiables

1. **Language**: C17
2. **API prefix**: `ktc_` (not `kinetic_` for public types/functions)
3. **Headers**: `include/ktc/`
4. **HTTP**: RFC 9110/9112, state machines, no llhttp/nghttp2 as libraries
5. **Config**: Simple YAML (`kinetic.name`, `listen_port`) — user implements during **tutorial T0** Eras 0–1; no Traefik/provider stack until asked
6. **Event loop**: libuv — user adds during **tutorial T0** (explore libuv, then wire listener)
7. **Scope**: Minimal diffs; author writes most code via tutorials

## Repository layout

```
include/ktc/core/     str.h, arena.h, config.h
include/ktc/version.h
src/core/             str.c, arena.c, parseyml.c
src/main.c            minimal entry (grows with tutorial T0)
docs/tutorials/       README.md (index), T0_foundation.md … T3_pipelining.md
docs/http1.1/         INVARIANTS.md
configs/example.yaml
configs/test_config.yaml
```

Library target: `ktc_core`. Binary: `kinetic`.

## Build & verify

```bash
make check
```

## Core types

- **`ktc_str`** — `{const uint8_t *ptr; size_t len}` for wire octets; non-owning
- **`ktc_arena`** — bump alloc; `create/destroy/alloc/calloc/reset`

Add `ktc_str_from_uv_buf` when the user wires libuv (tutorial T0 Era 1).

## Tutorial structure

| Doc | Content |
|-----|---------|
| [docs/tutorials/README.md](docs/tutorials/README.md) | Index and reading order |
| [T0_foundation.md](docs/tutorials/T0_foundation.md) | **Eras 0–3** — foundation through first lawful HTTP response |
| [T1_bodies.md](docs/tutorials/T1_bodies.md) | **Era 4** — body framing |
| [T2_persistence.md](docs/tutorials/T2_persistence.md) | **Era 5** — persistence |
| [T3_pipelining.md](docs/tutorials/T3_pipelining.md) | **Era 6** — pipelining |

Current phase: **tutorial T0** — author implements Eras 0–3. Tutorials T1–T3 are **post–Era 3** only.

Do not implement Era N+1 features when the user is on Era N.

## What not to do

- Do not re-add libconfig, ConfigProvider, Router/Service structs, listener.c, connection.c unless explicitly requested for a tutorial era
- Do not commit
- Do not skip tutorial eras and ship a full server

## License

MIT
