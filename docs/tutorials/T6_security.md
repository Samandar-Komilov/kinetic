# T6 — Security Hardening: Osman guards the gates

*Era 9. The server works well under good clients. But the internet is hostile. Malicious request smuggling, response splitting, and buffer overflow attacks will try to crash Osman. He must harden the parser.*

Continue from [Tutorial T5 — Era 8](T5_expect.md). Index: [README](README.md).

---

## Story

Osman's HTTP/1.1 messaging engine parses incoming streams based on length descriptors. But what happens if a malicious client deliberately sends ambiguous headers to desynchronize upstream proxies? 

An attacker sends a request containing **both** a `Content-Length` header and a `Transfer-Encoding: chunked` header. If a frontend reverse proxy forwards the request based on `Content-Length` but Osman parses it using `Transfer-Encoding`, the backend interprets the trailing bytes of the request as a *second*, smuggled request. This is **HTTP Request Smuggling** (CL.TE or TE.CL).

Osman must enforce strict constraints on white spaces, reject line-folding, enforce absolute limits on header counts/sizes, and handle invalid syntax strictly by shutting down the socket.

---

## RFC grounding (2–3 sections)

| RFC | Section | What Osman must implement |
|-----|---------|-------------------------|
| [RFC 9112](https://www.rfc-editor.org/rfc/rfc9112.html) | **§11** | Security considerations. Focus on request desynchronization / smuggling. |
| [RFC 9112](https://www.rfc-editor.org/rfc/rfc9112.html) | **§2.2** | Strictly reject Line Folding (obs-fold) in headers with `400 Bad Request`. |
| [RFC 9110](https://www.rfc-editor.org/rfc/rfc9110.html) | **§5.5** | Whitespace handling. Reject whitespace between header name and colon (`Header : value`) with `400`. |

---

## Invariants (this era)

| ID | Rule |
|----|------|
| H11-SEC-001 | If a request contains both `Transfer-Encoding` and `Content-Length`, `Transfer-Encoding` takes precedence but the connection **MUST** be closed after the response |
| H11-SEC-002 | Reject any request header that contains whitespace between the field name and the colon with `400 Bad Request` and close the connection |
| H11-SEC-003 | Enforce strict timeouts on idle connections and request body read operations |
| H11-SEC-004 | Reject obsolete line folding (CRLF followed by space/tab) with `400 Bad Request` |

---

## What can go wrong

| Failure | Attack Vector | Solution |
|---------|---------------|----------|
| Slowloris | Keeps socket open by sending headers extremely slowly | Implement read/idle timeouts |
| Header Overflow | Large header blocks exhaust arena memory | Enforce a hard 8KB header limit |
| Smuggling | Ambiguous lengths trick downstream proxies | Reject invalid chunk sizes and mismatched CL/TE headers |

---

## Exit criteria

- [ ] A request with space before colon (e.g. `Host : localhost`) is rejected with `400 Bad Request`.
- [ ] Obsolete line folding gets rejected with `400 Bad Request`.
- [ ] Mismatched / duplicate `Content-Length` headers trigger a `400` or connection teardown.
- [ ] Max header size limits are strictly enforced.
- [ ] Regression check: Eras 0–8 tests pass.

**Osman's diary:** *Trust no one. If a sentence has two lengths, it is a lie. Shut the gate.*
