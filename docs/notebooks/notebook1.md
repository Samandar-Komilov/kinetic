# Notebook 1 — Sam learns message bodies

*Era 4. Era 3 ended at the empty line after headers — no body, no `POST`. That changes now.*

Continue from [Notebook 0 — Era 3](notebook0.md). Index: [README](README.md).

---

## Story

Sam's server speaks HTTP/1.1 on the front half: request-line, headers, `Host`, a lawful `200` with `Connection: close`. Clients were polite — `GET` with no body.

Now a client sends:

```http
POST /upload HTTP/1.1\r\n
Host: localhost\r\n
Content-Length: 5\r\n
\r\n
hello
```

After the header section's empty line, **five more octets** belong to this message. If Sam stops reading or mis-counts, the next request on a persistent connection (later) will parse garbage as a request-line — a smuggling-class bug.

Sam must learn **message body framing**: how many bytes follow the headers, and when there are none.

---

## RFC grounding (2–3 sections)

| RFC | Section | What Sam must implement |
|-----|---------|-------------------------|
| [RFC 9112](https://www.rfc-editor.org/rfc/rfc9112.html) | **§6.2–§6.3** | `Content-Length`; **§6.3 precedence** — the ordered rules that decide body length |
| [RFC 9112](https://www.rfc-editor.org/rfc/rfc9112.html) | **§7.1** | Chunked transfer coding — decode until zero chunk |
| [RFC 9110](https://www.rfc-editor.org/rfc/rfc9110.html) | **§8.6** | `Content-Length` on responses; when it must not appear |

### §6.3 precedence (implement exactly this order)

1. `Transfer-Encoding: chunked` (final) → decode chunks.
2. TE in request, chunked not final → **400**, close.
3. HEAD / 1xx / 204 / 304 → no body.
4. 2xx CONNECT → tunnel (defer if not implemented).
5. Both TE and CL → TE wins; treat as error; **close** after response.
6. Invalid CL → **400**, close.
7. Valid CL, no TE → read exactly CL octets.
8. Request, none of above → body length **0**.
9. Response, none of above → read until close (avoid for your responses).

Sam does not invent a shortcut list. The RFC order **is** the state machine.

---

## Invariants (this era)

| ID | Rule |
|----|------|
| H11-FRAME-001 | Implement §6.3 precedence exactly |
| H11-FRAME-002 | TE present, chunked not final → **400**, close |
| H11-FRAME-003 | Invalid CL in request → **400**, close |
| H11-FRAME-004 | Both TE and CL → process per TE or reject; **always close** |
| H11-FRAME-009–011 | No CL/TE on 1xx/204/CONNECT response |
| H11-CHUNK-001 | Must parse chunked coding |
| H11-CHUNK-003 | Guard hex chunk-size overflow |
| H11-SEC-003 | CL+TE ambiguity = smuggling surface |

---

## Parser extension (after Era 3 header section)

```text
HEADERS_COMPLETE
  → RESOLVE_FRAMING    (apply §6.3 to request)
  → REQUEST_BODY       (read N octets or decode chunked)
  → BODY_COMPLETE
  → DISPATCH
```

For Era 4 on **requests**:

- Extend connection buffer logic: after headers, either `body_remaining = CL` or enter `CHUNKED` sub-FSM.
- Do not dispatch to handlers until body is fully consumed or absent.
- `ktc_str` slices into body are valid only while buffer holds those octets; for handlers, copy into `ktc_arena` if needed beyond one read.

Chunked decode pseudo-loop ([9112 §7.1.3](https://www.rfc-editor.org/rfc/rfc9112.html#section-7.1.3)):

```text
read chunk-size (hex) + CRLF
while size > 0: read chunk-data + CRLF
read zero chunk + optional trailers + CRLF
```

---

## Responses with bodies (minimal)

Era 3 sent `Content-Length: 0`. Now Sam may send a small body:

```http
HTTP/1.1 200 OK\r\n
Date: ...\r\n
Content-Length: 2\r\n
Connection: close\r\n
\r\n
OK
```

| ID | Rule |
|----|------|
| H11-FRAME-007 | Prefer length-delimited responses over close-delimited |
| H11-HDR-007 | Still send `Date` on 2xx |

---

## What Sam must not do yet

- Keep-alive across requests ([notebook 2](notebook2.md)).
- Pipelining ([notebook 3](notebook3.md)).
- Trailers (optional later; ignore unrecognized chunk extensions per `H11-CHUNK-002`).

---

## Exit criteria

- [ ] `POST` with `Content-Length: 5` — body consumed exactly; handler sees 5 octets.
- [ ] Chunked request decoded to correct byte count.
- [ ] `Content-Length` + `Transfer-Encoding` on request → **400** and connection closed.
- [ ] Invalid CL → **400**.
- [ ] `GET` still works with no body (regression from Era 3).
- [ ] Unit tests for §6.3 edge cases without live network.

**Sam's diary:** *I know where the header ends and the body begins. Every octet is accounted for.*

---

## Next

[Notebook 2 — Persistence](notebook2.md): stop closing after every response; drain bodies so the next request parses cleanly.
