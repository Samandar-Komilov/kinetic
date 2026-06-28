# KinetiC

A high-performance, cross-platform HTTP/1.1 server in C — built incrementally, RFC by RFC, with you writing every layer.

## Status

**Bootstrap only.** The repo ships:

- `ktc_str` — octet slice for wire data
- `ktc_arena` — bump allocator for connection/request scope
- A minimal `kinetic` binary (prints version)
- [Notebook series](docs/notebooks/README.md) — Sam's evolution story

No config parser, no TCP listener, no HTTP parser in tree yet — follow [notebook 0](docs/notebooks/notebook0.md).

## Build

```bash
make build
make test
./build/src/kinetic
```

## Learning path

See [docs/notebooks/README.md](docs/notebooks/README.md) for the full arc.

| Notebook | Era | Topic |
|----------|-----|-------|
| [0 — Foundation](docs/notebooks/notebook0.md) | 0–3 | Connection, request-line, headers, first `200` |
| [1 — Bodies](docs/notebooks/notebook1.md) | 4 | Content-Length, chunked, §6.3 |
| [2 — Persistence](docs/notebooks/notebook2.md) | 5 | Keep-alive, graceful close |
| [3 — Pipelining](docs/notebooks/notebook3.md) | 6 | Ordered responses |

RFC checklist: [docs/http1.1/INVARIANTS.md](docs/http1.1/INVARIANTS.md)

## Layout

```
include/ktc/core/     ktc_str, ktc_arena
src/core/             implementations
src/main.c            your entry point grows here
configs/kinetic.yaml.example   minimal YAML (you parse in notebook 0 Era 1)
docs/notebooks/       evolution story
```

## Requirements

- CMake 3.20+, C17
- libuv / libyaml — added when **you** implement notebook 0 (CMake can fetch libuv)

## Format & lint

```bash
make format
make check
```

## License

MIT — see [LICENSE](LICENSE).
