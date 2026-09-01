# T4 — Methods & Semantics: Osman respects the verbs

*Era 7. We can parse lines, headers, bodies, keep the connection open, and even handle pipelined requests. But until now, we treated every request like a GET. It is time to respect HTTP verbs.*

Continue from [Tutorial T3 — Era 6](T3_pipelining.md). Index: [README](README.md).

---

## Story

Osman's server is fast, persistent, and pipeline-lawful. However, whether the client sends `GET /`, `POST /`, or `DELETE /`, the server behaves identically: it parses the request, ignores the method semantics, and sends a `200 OK` back.

If a client sends `HEAD /logo.png`, they want to verify the resource exists and fetch its headers (like `Content-Length` or `Content-Type`) *without* downloading the body bytes. If Osman responds with the full body anyway, he wastes bandwidth and violates the HTTP/1.1 spec.

Furthermore, if a client sends `OPTIONS *`, the server must announce its capabilities. Era 7 is about teaching Osman the semantic meanings of standard HTTP methods.

---

## RFC grounding (2–3 sections)

| RFC | Section | What Osman must implement |
|-----|---------|-------------------------|
| [RFC 9110](https://www.rfc-editor.org/rfc/rfc9110.html) | **§9.3.1 & §9.3.2** | `GET` and `HEAD` semantics. `HEAD` response **MUST** contain headers as if it were a `GET`, but **MUST NOT** include a response body. |
| [RFC 9110](https://www.rfc-editor.org/rfc/rfc9110.html) | **§9.3.7** | `OPTIONS` semantics. Respond with supported methods in the `Allow` header field. |
| [RFC 9110](https://www.rfc-editor.org/rfc/rfc9110.html) | **§9.1** | Safe and Idempotent definitions. Standard methods classification. |

---

## Invariants (this era)

| ID | Rule |
|----|------|
| H11-METH-001 | `HEAD` response headers must match `GET` headers but include **no body** |
| H11-METH-002 | `OPTIONS` requests must receive a response listing supported methods in `Allow` header |
| H11-METH-003 | Reject unsupported or unsafe methods with `501 Not Implemented` or appropriate status codes |

---

## Implementation Sequence

1. **Method Routing:**
   Map the parsed method string (`c->req_line_parser.method`) to its semantic handler.
2. **HEAD Optimization:**
   Ensure formatting logic skips the actual body write if the method was `HEAD`, while keeping correct headers like `Content-Length`.
3. **OPTIONS Handler:**
   Intercept `OPTIONS` and reply with `Allow: GET, HEAD, POST, OPTIONS`.

---

## Exit criteria

- [ ] `HEAD /` request receives the exact same header set as `GET /` (including `Content-Length`), but no payload bytes follow.
- [ ] `OPTIONS *` or `OPTIONS /` returns `200 OK` or `204 No Content` with an `Allow` header listing supported verbs.
- [ ] Any method not implemented by the server (e.g. `PATCH`, `TRACE`) receives a `501 Not Implemented` response.
- [ ] Regression check: Eras 0–6 tests pass.

**Osman's diary:** *A verb is not just a label. It is a promise of what I will or will not send back.*
