# HTTP/1.1 Message Body Framing Analysis

This document compares the stateful, RFC-compliant body framing parser of **Kinetic** with the string-parsing and heap-copying techniques seen in the `cserve` reference project.

---

## 1. Request Body Framing Comparison

### Slicing & Copying (`cserve` / `parse_http_request`)
The old approach looks up `Content-Length` via a header array sweep using `strncmp` on heap-duplicated header strings and parses the value using `atoi`:
```c
// Extracting body length
for (int j = 0; j < req->header_count; j++) {
    if (strncmp(req->headers[j].name, "Content-Length", req->headers[j].name_len) == 0) {
        req->body_len = atoi(req->headers[j].value); // Unsafe parsing
        break;
    }
}
req->body = (char *)ptr;
```

#### Vulnerabilities & RFC Violations:
1. **Ambiguous Framing & Smuggling (`H11-SEC-003`, `H11-FRAME-004`)**:
   It ignores the `Transfer-Encoding` header entirely. If a client sends both `Content-Length` and `Transfer-Encoding: chunked`, this mismatch passes undetected, leaving the server vulnerable to HTTP Request Smuggling where upstream proxies route requests differently than the backend.
2. **Precedence Violations (`H11-FRAME-001`)**:
   It does not enforce RFC 9112 §6.3 precedence rules. There is no check to ensure GET or HEAD requests omit bodies, nor does it enforce error states for non-chunked transfer encodings.
3. **Invalid `Content-Length` Processing (`H11-FRAME-003`)**:
   Using `atoi` on unvalidated strings is unsafe. If `Content-Length` contains letters (`15abc`), is negative (`-50`), or overflows, `atoi` yields undefined/garbage integers. An attacker could exploit this to trigger massive allocations or infinite socket waiting.
4. **Lack of Chunked Coding (`H11-CHUNK-001`)**:
   Chunked payloads are not decoded. The raw hex size markers and CRLFs are treated as raw request body content, corrupting the application payload.

### Kinetic Stateful Parser (`ktc_body_parser_feed`)
Kinetic resolves framing using strict §6.3 precedence filters:
* Rejects conflicting `CL` and `TE` headers immediately with `400 Bad Request`.
* Validates `Content-Length` to contain exclusively digits and guards against integer overflows.
* Decodes chunked payloads in-place using a character-by-character sub-FSM, checking for hex chunk-size overflow (`H11-CHUNK-003`) and ignoring unrecognized chunk extensions (`H11-CHUNK-002`).

---

## 2. Code Card Comparison (PostSpark Ready)

### Snippet 1: The Old Slicing Way (cserve)
```c
// ❌ Old Slicing: Susceptible to Smuggling and Invalid Inputs
int parse_http_request(const char *data, size_t len, HTTPRequest *req) {
    // Non-incremental: assumes entire request is already contiguous in memory
    int consumed = parse_request_line(req, data, len);
    
    // Unsafe Content-Length sweep: ignores Transfer-Encoding entirely
    for (int j = 0; j < req->header_count; j++) {
        if (strncmp(req->headers[j].name, "Content-Length", 14) == 0) {
            req->body_len = atoi(req->headers[j].value); // Unsafe conversion
            break;
        }
    }
    req->body = (char *)(data + headers_end); // No chunked decoding support
    return 0;
}
```

### Snippet 2: The Kinetic Stateful Framing Way
```c
//  New Framing: Stateful §6.3 compliant resolution
bool ktc_body_resolve_framing(ktc_body_parser_t *bp, const ktc_header_parser_t *hp, ktc_str method) {
    bool has_te = has_header(hp, "Transfer-Encoding");
    bool has_cl = has_header(hp, "Content-Length");

    if (has_te && has_cl) return false; // Guard smuggling (H11-SEC-003)
    if (ktc_str_eq_case_insensitive(method, "HEAD")) return true;

    if (has_te) {
        if (last_token_is(hp, "chunked")) bp->framing = FRAMING_CHUNKED;
        else return false; // TE present but chunked not final (H11-FRAME-002)
    } else if (has_cl) {
        if (!parse_cl_safe(cl_header_val, &bp->content_length)) return false;
        bp->framing = FRAMING_LENGTH;
    }
    return true;
}
```
