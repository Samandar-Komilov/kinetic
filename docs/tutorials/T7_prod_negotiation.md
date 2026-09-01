# T7 — Production Readiness: Osman prepares for the open web

*Era 10+. With messaging, semantics, and security established, Osman is ready for real-world deployment. This era introduces compression (gzipping), caching headers, TLS encryption, and performance multiplexing.*

Continue from [Tutorial T6 — Era 9](T6_security.md). Index: [README](README.md).

---

## Story

Osman's server is safe and robust, but text-heavy HTML/CSS payloads are slow to transfer. Furthermore, clients are re-requesting static assets (like `/logo.png`) on every visit, consuming CPU cycles and bandwidth. 

To solve this, Osman must implement **Content Encoding (gzipping)** to compress data on the fly, and **HTTP Caching** to let clients reuse previously downloaded files. Finally, to deploy on the modern web, Osman must wrap the socket stream in a **TLS layer** and scale performance using a multi-worker event loop architecture.

---

## Where Caching and Compression Live

In the HTTP/1.1 lifecycle, caching and compression occur after parsing the request headers and before flushing the final response body to the network:

```text
CLIENT REQUEST 
   │
   ▼
[Parser State Machine] ──► [HTTP Handler / Router]
                                 │
                                 ▼
                    [Content Coding / Compression]  <── (Gzip / Deflate check)
                                 │
                                 ▼
                     [Caching Layer (RFC 9111)]     <── (ETag / Cache-Control)
                                 │
                                 ▼
                       [TLS Encryption Layer]       <── (OpenSSL / MbedTLS wrapper)
                                 │
                                 ▼
                            TCP SOCKET
```

---

## RFC grounding (Content Coding & Caching)

| RFC | Section | What Osman must implement |
|-----|---------|-------------------------|
| [RFC 9110](https://www.rfc-editor.org/rfc/rfc9110.html) | **§8.4** | Content Codings. Parse the client's `Accept-Encoding` header. If it contains `gzip`, compress the response body and set `Content-Encoding: gzip`. |
| [RFC 9111](https://www.rfc-editor.org/rfc/rfc9111.html) | **§3 & §4** | HTTP Caching. Handle conditional requests: check `If-None-Match` against resource `ETag` or `If-Modified-Since` against `Last-Modified`. Respond with `304 Not Modified` if valid. |
| [RFC 9111](https://www.rfc-editor.org/rfc/rfc9111.html) | **§5.2** | Caching Controls. Populate the `Cache-Control` header to control downstream caches. |

---

## Architecture Sketch

### 1. Compression Layer (Gzip)
When handling a request:
1. Examine `Accept-Encoding` header field.
2. If `gzip` is supported by the client, buffer the response body, compress it (e.g. using `zlib`), recalculate the compressed `Content-Length`, set `Content-Encoding: gzip`, and write the compressed payload.

### 2. Caching Layer (Conditional GET)
To avoid transferring bytes at all:
1. Generate an `ETag` (e.g., hash of file content/metadata) for static files.
2. If the client request contains `If-None-Match: "etag_value"`, compare it.
3. If they match, intercept the handler response, change the status to `304 Not Modified`, and write a bodyless header-only response.

### 3. Multi-worker & TLS (Roadmap)
- **TLS**: Intercept read and write streams using OpenSSL/MbedTLS callbacks before passing data to libuv buffers.
- **Workers**: Use `SO_REUSEPORT` at the socket level so multiple libuv loops on separate thread workers can balance incoming TCP connections.

---

## Exit criteria

- [ ] A client request with `Accept-Encoding: gzip` for a compressible asset receives a gzipped response with `Content-Encoding: gzip`.
- [ ] Subsequent requests for the same asset with `If-None-Match` match the server's `ETag` and receive a `304 Not Modified` with no body bytes.
- [ ] TLS handshakes complete successfully, serving traffic over HTTPS.

**Osman's diary:** *Compress the letters, remember what they read yesterday, and seal it in a secure envelope. Now I am ready.*
