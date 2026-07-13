# Connection & Loop Lifecycle Management Analysis

This document compares event-driven, non-blocking connection management in **Kinetic** with the non-blocking `epoll` loop model implemented in the `cserve` reference project.

---

## 1. Loop Model & Lifecycle Flow

```
1. Epoll Non-Blocking Model (cserve):
   [epoll_wait] -> Accept client fd -> Set client fd non-blocking (fcntl O_NONBLOCK)
        |
        +-> Find slot in static connections pool (MAX_CONNECTIONS limit)
        +-> Register to epoll (EPOLLIN)
        +-> Read data -> parse -> write -> close fd & DEL from epoll

2. Event-Driven Loop (Kinetic/libuv):
   [uv_listen]
        |
        v
   [on_connection] -> Allocates ktc_conn_t & ktc_arena_t (Dynamic Scaling)
        |
   [on_read] (Async callbacks) -> Accumulate -> parse request
        |
   [send_lawful_response] -> writes HTTP data asynchronously
        |
   [on_write] -> Close client handle -> [on_handle_closed] (Clean reference count)
```

### Direct Linux `epoll` (`cserve`)
* **Platform Lock-in**: Directly calls `epoll_create1`, `epoll_ctl`, and `epoll_wait`. It is non-portable and will not compile on macOS (kqueue) or Windows (IOCP).
* **Static Connection Pool**: Relies on a fixed-size connection array (`MAX_CONNECTIONS`). If all slots are full, new connections are immediately dropped:
  ```c
  if (!conn || self->active_count >= MAX_CONNECTIONS) {
      close(client_fd);
      continue;
  }
  ```
* **Explicit Socket Configuration**: Manually handles non-blocking descriptor conversion via `fcntl(client_fd, F_SETFL, flags | O_NONBLOCK)`.
* **Shutdown Strategy**: Closes the `epoll_fd` and manually loops through the connections pool to clean up allocations.

### Kinetic libuv Loop
* **Multi-Platform Portability**: Leverages `libuv`'s event loop (`uv_run`), which dynamically compiles to `epoll` on Linux, `kqueue` on macOS/BSD, and `IOCP` on Windows.
* **Dynamic Connection Allocation**: Allocates connection structures dynamically wrapped in context-specific arenas (`ktc_arena_t`), allowing the server to scale memory resources to actual connection volumes.
* **Lifecycle Reference Counting**: Employs `pending_closes_cnt` to prevent use-after-free bugs. The connection structure is only deallocated when all associated event loop handles (sockets, timers) have completed their asynchronous `uv_close` sequences.

---

## 2. Allocation Architecture Comparison

* **`cserve`**: Allocates socket read buffers on the heap and copies every parsed token (method, URI, header names, values) using `strndup`:
  ```c
  req_t->request_line.method = strndup(ptr, space - ptr);
  header->name = strndup(ptr, colon - ptr);
  ```
  This creates extensive malloc/free pressure on the system allocator for every incoming request.
* **Kinetic**: Parses request tokens into zero-copy, non-owning views (`ktc_str`). It keeps references inside the connection buffer without a single heap allocation or copy, significantly reducing allocation overhead.
