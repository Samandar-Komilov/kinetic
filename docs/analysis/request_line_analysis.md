# Writing a Compliant HTTP/1.1 Parser: Lessons in State Machines and RFC Invariants

This post outlines the architectural shift from a naive string-scanning HTTP request-line parser to a robust, stateful, and RFC-compliant Finite State Machine (FSM).

---

## 1. The Naive Approach vs. The Compliant Approach

Here is a side-by-side comparison of the core architectural differences:

| Feature | Old Code (`parse_request_line`) | Current Code (`ktc_req_line_parser_feed`) |
| :--- | :--- | :--- |
| **Parsing Model** | **Contiguous Buffer**: Assumes the entire request line is present in memory at once. | **Incremental FSM**: Processes incoming network data byte-by-byte as it arrives. |
| **TCP Fragmentation** | **Fragile**: Returns `-1` if the line is split across multiple TCP packets. | **Robust**: Resumes parsing exactly where it left off, maintaining state in a context structure. |
| **Delimiter Strictness**| **Lenient**: Searches for the first space. `GET  /` (multiple spaces) parses incorrectly, risking HTTP Request Smuggling. | **Strict**: Rejects double spaces, tabs, or unexpected control characters immediately with `400 Bad Request`. |
| **Security Limits** | **None**: A giant URI with no spaces will cause infinite scanning or memory overflows. | **Guarded**: Limits URI length to `8KB` (`414 URI Too Long`) and method length to `32B` (`501 Not Implemented`). |
| **RFC 9112 §3** | **Ignored**: Fails if the client sends leading empty lines (`\r\n`) to keep connections alive. | **Compliant**: Gracefully skips leading blank lines before beginning method extraction. |

---

## 2. Key Insights & Common Pitfalls

1. **The Fallacy of Stream Completeness**: Network sockets deliver byte streams, not finished lines. Building parsers that assume contiguous, already-complete strings forces blocking reads or fragile code.
2. **HTTP Request Smuggling**: Lenient whitespace parsing is a primary vector for request smuggling. Strictly validating that exactly one `SP` character separates tokens is a security requirement, not an aesthetic preference.
3. **DoS Vector Prevention**: Without length constraints on individual tokens, your parser is vulnerable to resource exhaustion.

---

## 3. Code Card Comparison (PostSpark Ready)

### Snippet 1: The Old Way (Fragile & Lenient)
```c
// ❌ Old Way: Simple memchr scanning (Fragile)
int parse_request_line(HTTPRequest *req, const char *buf, size_t len) {
    const char *ptr = buf;
    const char *end = buf + len;

    // Fails if TCP packet is split mid-line
    const char *space1 = memchr(ptr, ' ', end - ptr);
    if (!space1) return -1;
    req->method = ptr;
    req->method_len = space1 - ptr;

    // Vulnerable: Double spaces or tabs pass unvalidated
    ptr = space1 + 1;
    const char *space2 = memchr(ptr, ' ', end - ptr);
    if (!space2) return -1;
    req->uri = ptr;
    
    // No size validation: susceptible to DoS attacks
    return space2 - buf;
}
```

### Snippet 2: The New Way (Stateful & Robust)
```c
//  New Way: Incremental FSM (Safe & Compliant)
bool ktc_req_line_parser_feed(ktc_req_line_parser_t *p, const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        uint8_t c = data[i];
        switch (p->state) {
            case STATE_METHOD:
                if (c == ' ') { // End token
                    p->state = STATE_TARGET;
                } else if (!is_token_char(c) || p->len > 32) {
                    p->error = ERR_BAD_SYNTAX; // Enforce RFC token chars
                    return false;
                }
                break;
            case STATE_TARGET:
                // Incremental checks for exact single-spaces & 8KB URI limits
                if (c == ' ') p->state = STATE_VERSION;
                else if (p->len > 8192) return error(p, ERR_URI_TOO_LONG);
                break;
        }
    }
    return true; // Continues parsing across split packets
}
```
