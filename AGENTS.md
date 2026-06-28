# KinetiC — Agent Guide

> MANDATORY: Do not commit! The author reviews and then commits always!

## What this project is

**KinetiC** — HTTP/1.1 server in C, built by the author following `docs/notebooks/`. The repo intentionally stays thin: only `ktc_str` and `ktc_arena` helpers exist. **Do not** add config parsers, net modules, or HTTP parsers unless the user asks for a specific notebook era.

## Non-negotiables

1. **Language**: C17
2. **API prefix**: `ktc_` (not `kinetic_` for public types/functions)
3. **Headers**: `include/ktc/`
4. **HTTP**: RFC 9110/9112, state machines, no llhttp/nghttp2 as libraries
5. **Config**: Simple YAML (`kinetic.name`, `listen_port`) — user implements during **notebook 0** Eras 0–1; no Traefik/provider stack until asked
6. **Event loop**: libuv — user adds during **notebook 0** (explore libuv, then wire listener)
7. **Scope**: Minimal diffs; author writes most code via notebooks

## Repository layout

```
include/ktc/core/     str.h, arena.h
include/ktc/version.h
src/core/             str.c, arena.c
src/main.c            minimal entry (grows with notebook 0)
docs/notebooks/       README.md (index), notebook0.md … notebook3.md
docs/http1.1/         INVARIANTS.md
configs/kinetic.yaml.example
```

Library target: `ktc_core`. Binary: `kinetic`.

## Build & verify

```bash
make check
```

## Core types

- **`ktc_str`** — `{const uint8_t *ptr; size_t len}` for wire octets; non-owning
- **`ktc_arena`** — bump alloc; `create/destroy/alloc/calloc/reset`

Add `ktc_str_from_uv_buf` when the user wires libuv (notebook 0 Era 1).

## Notebook structure

| Doc | Content |
|-----|---------|
| [docs/notebooks/README.md](docs/notebooks/README.md) | Index and reading order |
| [notebook0.md](docs/notebooks/notebook0.md) | **Eras 0–3** — foundation through first lawful HTTP response |
| [notebook1.md](docs/notebooks/notebook1.md) | **Era 4** — body framing |
| [notebook2.md](docs/notebooks/notebook2.md) | **Era 5** — persistence |
| [notebook3.md](docs/notebooks/notebook3.md) | **Era 6** — pipelining |

Current phase: **notebook 0** — author implements Eras 0–3. Notebooks 1–3 are **post–Era 3** only.

Do not implement Era N+1 features when the user is on Era N.

## What not to do

- Do not re-add libconfig, ConfigProvider, Router/Service structs, listener.c, connection.c unless explicitly requested for a notebook era
- Do not commit
- Do not skip notebook eras and ship a full server

## License

MIT
