# T5 — Expect: Osman checks the price

*Era 8. Clients want to send huge payloads, but they do not want to upload gigabytes only to get a 403 Forbidden. They send a questionnaire header. Osman must answer.*

Continue from [Tutorial T4 — Era 7](T4_methods.md). Index: [README](README.md).

---

## Story

As Osman's server handles file uploads (`POST` and `PUT` with body framing from T1), clients start uploading larger and larger datasets. 

A client wants to upload a 500MB video to `/restricted-zone`. If the user is unauthenticated, Osman will reject the request with `401 Unauthorized` or `403 Forbidden`. If the client sends the entire 500MB payload before Osman evaluates the headers, 500MB of network bandwidth is wasted, only for the client to receive an error page.

To prevent this, the client sends:

```http
POST /restricted-zone HTTP/1.1\r\n
Host: localhost\r\n
Content-Length: 524288000\r\n
Expect: 100-continue\r\n
\r\n
```

At this point, the client stops writing. It waits for Osman to say "Go ahead" or "No". 
If Osman likes the headers, he sends an intermediate informational response:

```http
HTTP/1.1 100 Continue\r\n\r\n
```

Upon receiving this, the client sends the remaining 500MB body. If Osman does not support this and stays silent, the client might timeout or experience a slow upload. If Osman immediately sends a `403 Forbidden` response, the client aborts.

---

## RFC grounding (2–3 sections)

| RFC | Section | What Osman must implement |
|-----|---------|-------------------------|
| [RFC 9110](https://www.rfc-editor.org/rfc/rfc9110.html) | **§10.1.1** | The `Expect` header field. Handle `100-continue`. |
| [RFC 9110](https://www.rfc-editor.org/rfc/rfc9110.html) | **§15.2.1** | `100 Continue` status code usage. |
| [RFC 9110](https://www.rfc-editor.org/rfc/rfc9110.html) | **§15.5.18** | `417 Expectation Failed` for unknown expectation extensions. |

---

## Invariants (this era)

| ID | Rule |
|----|------|
| H11-EXP-001 | If `Expect: 100-continue` is present, the server must either send `100 Continue` or an error status before reading the body |
| H11-EXP-002 | Do not read body bytes from the TCP socket until the `100 Continue` is sent |
| H11-EXP-003 | If the expectation is unknown or cannot be met, respond with `417 Expectation Failed` |

---

## Implementation Sequence

1. **Expectation Check:**
   In header processing (`handle_headers`), check if the client sent `Expect: 100-continue`.
2. **Intermediate Write:**
   If authentication or headers are valid, write a raw `HTTP/1.1 100 Continue\r\n\r\n` response to the client socket. Do NOT end or close the request context.
3. **Resume Reading:**
   Once the `100 Continue` is flushed, resume the socket read to consume the body payload.

---

## Exit criteria

- [ ] Client sends a `POST` with `Expect: 100-continue`.
- [ ] Server responds with `HTTP/1.1 100 Continue\r\n\r\n` first, then reads the body, and finally sends `200 OK`.
- [ ] Reject invalid headers with `401`/`403` immediately without sending `100 Continue`, closing the connection.
- [ ] Regression check: Eras 0–7 tests pass.

**Osman's diary:** *Do not let them dump cargo on my dock until I have checked their manifests.*
