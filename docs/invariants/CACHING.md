# HTTP Caching Invariants (RFC 9111)

This document is the **normative checklist** for KinetiC’s HTTP Caching implementation, derived directly from **[RFC 9111](https://www.rfc-editor.org/rfc/rfc9111.html)** (HTTP Caching, June 2022).

---

## 1. Canonical References

| Document | Role |
|----------|------|
| [RFC 9111](https://www.rfc-editor.org/rfc/rfc9111.html) | HTTP Caching (stores, freshness, validation, `Cache-Control` directives) |
| [RFC 9110](https://www.rfc-editor.org/rfc/rfc9110.html) | HTTP Semantics (§13 Conditional Requests, §8.8 Validators) |
| [RFC 2119](https://www.rfc-editor.org/rfc/rfc2119.html) / [RFC 8174](https://www.rfc-editor.org/rfc/rfc8174.html) | Normative keywords (MUST, SHOULD, MAY) |

Each invariant below has an ID prefix `CACHE-` for tracking in `STATE.md` and test suites.

---

## 2. Storing Responses in Caches

Derived from [RFC 9111 §3](https://www.rfc-editor.org/rfc/rfc9111.html#section-3).

| ID | Level | Invariant | RFC Basis |
|----|-------|-----------|-----------|
| CACHE-STORE-001 | MUST NOT | Store any part of a response if the request or response contains `Cache-Control: no-store`. | [9111 §3](https://www.rfc-editor.org/rfc/rfc9111.html#section-3), [§5.2.2.5](https://www.rfc-editor.org/rfc/rfc9111.html#section-5.2.2.5) |
| CACHE-STORE-002 | MUST NOT | Store a response to a request with an unrecognized or unsafe method unless explicitly allowed by cache extensions. | [9111 §3](https://www.rfc-editor.org/rfc/rfc9111.html#section-3) |
| CACHE-STORE-003 | MUST NOT | Store a response with `private` directive in a shared (proxy) cache. | [9111 §5.2.2.7](https://www.rfc-editor.org/rfc/rfc9111.html#section-5.2.2.7) |
| CACHE-STORE-004 | MUST NOT | Store a response to an authenticated request (`Authorization` header) in a shared cache unless `public`, `must-revalidate`, or `s-maxage` is present. | [9111 §3.5](https://www.rfc-editor.org/rfc/rfc9111.html#section-3.5) |
| CACHE-STORE-005 | MUST | Use the request target URI combined with selected request headers (`Vary`) as the primary cache key. | [9111 §2](https://www.rfc-editor.org/rfc/rfc9111.html#section-2), [§4.1](https://www.rfc-editor.org/rfc/rfc9111.html#section-4.1) |
| CACHE-STORE-006 | MUST NOT | Store a response if the `Vary` header contains `*`. | [9111 §4.1](https://www.rfc-editor.org/rfc/rfc9111.html#section-4.1) |

---

## 3. Constructing Responses from Caches (Freshness)

Derived from [RFC 9111 §4.2](https://www.rfc-editor.org/rfc/rfc9111.html#section-4.2).

| ID | Level | Invariant | RFC Basis |
|----|-------|-----------|-----------|
| CACHE-FRESH-001 | MUST | Calculate response freshness lifetime using the precedence: `s-maxage` (shared cache) > `max-age` > `Expires` > heuristic freshness. | [9111 §4.2.1](https://www.rfc-editor.org/rfc/rfc9111.html#section-4.2.1) |
| CACHE-FRESH-002 | MUST NOT | Serve a stale response unless explicitly disconnected, authorized by client `max-stale`, or serving a stale fallback permitted by `stale-if-error`/`stale-while-revalidate`. | [9111 §4.2.4](https://www.rfc-editor.org/rfc/rfc9111.html#section-4.2.4) |
| CACHE-FRESH-003 | MUST NOT | Serve a stale response if the cached response contains `must-revalidate` or `proxy-revalidate` when unable to validate with origin. | [9111 §5.2.2.2](https://www.rfc-editor.org/rfc/rfc9111.html#section-5.2.2.2) |
| CACHE-FRESH-004 | MUST | Generate an accurate `Age` header field when serving a response from cache. | [9111 §5.1](https://www.rfc-editor.org/rfc/rfc9111.html#section-5.1) |
| CACHE-FRESH-005 | MUST NOT | Apply heuristic freshness to responses with query parameters unless explicit cache controls (`max-age`, `Expires`) exist. | [9111 §4.2.2](https://www.rfc-editor.org/rfc/rfc9111.html#section-4.2.2) |

---

## 4. Validation & Invalidation

Derived from [RFC 9111 §4.3 & §4.4](https://www.rfc-editor.org/rfc/rfc9111.html#section-4.3).

| ID | Level | Invariant | RFC Basis |
|----|-------|-----------|-----------|
| CACHE-VAL-001 | MUST | Revalidate cached response with origin server using `If-None-Match` (strong or weak `ETag`) or `If-Modified-Since` when freshness expires or on `no-cache`. | [9111 §4.3](https://www.rfc-editor.org/rfc/rfc9111.html#section-4.3) |
| CACHE-VAL-002 | MUST | On receiving `304 (Not Modified)` from origin, update cached response headers and freshness metadata before serving. | [9111 §4.3.4](https://www.rfc-editor.org/rfc/rfc9111.html#section-4.3.4) |
| CACHE-VAL-003 | MUST | Invalidate cached entries for the target URI when a successful non-safe request (`POST`, `PUT`, `DELETE`) is received. | [9111 §4.4](https://www.rfc-editor.org/rfc/rfc9111.html#section-4.4) |
| CACHE-VAL-004 | MUST | Invalidate URI locations specified in `Location` and `Content-Location` response headers for unsafe requests. | [9111 §4.4](https://www.rfc-editor.org/rfc/rfc9111.html#section-4.4) |

---

## 5. `Cache-Control` Directives

Derived from [RFC 9111 §5.2](https://www.rfc-editor.org/rfc/rfc9111.html#section-5.2).

### 5.1 Request Directives
* **`no-cache`**: Cache MUST NOT use a cached response to satisfy the request without successful validation with origin.
* **`no-store`**: Cache MUST NOT store any part of the request or resulting response.
* **`max-age=N`**: Client is unwilling to accept a response whose age exceeds $N$ seconds.
* **`max-stale[=N]`**: Client accepts a stale response up to $N$ seconds past expiration.
* **`min-fresh=N`**: Client requires a response that will remain fresh for at least $N$ seconds.
* **`only-if-cached`**: Cache MUST respond using only cached data; if no fresh cached data exists, respond `504 (Gateway Timeout)`.

### 5.2 Response Directives
* **`must-revalidate`**: Stale cached response MUST NOT be used without origin revalidation.
* **`no-cache`**: Response MAY be cached but MUST NOT be served without revalidation.
* **`no-store`**: Cache MUST NOT store any part of the response.
* **`public`**: Response MAY be stored by any cache (including shared proxy caches).
* **`private`**: Response is intended for a single user; MUST NOT be stored by shared caches.
* **`s-maxage=N`**: Overrides `max-age` and `Expires` for shared caches.
* **`immutable`**: Indicates origin will not update response body during freshness lifetime.
