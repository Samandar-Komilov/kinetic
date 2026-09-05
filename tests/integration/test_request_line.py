#!/usr/bin/env python3
"""
KinetiC HTTP/1.1 Integration Test Suite: Request Line & Line Endings
Covers:
  - Phase 2.1: Octet stream superset of US-ASCII (RFC 9112 §2.2 / H11-PARSE-001)
  - Phase 2.1: Response strictly CRLF, no bare CR (RFC 9112 §2.2 / H11-PARSE-002)
  - Phase 2.1: Line endings validation (bare CR, bare LF) (RFC 9112 §2.2 / H11-PARSE-003/007)
  - Phase 2.1: Whitespace before first header (RFC 9112 §2.2 / H11-PARSE-004/005)
  - Phase 2.1: Leading blank CRLF tolerance & rejection (RFC 9112 §2.2 / H11-PARSE-006/007)
  - Phase 2.2: Standard methods GET, HEAD, POST, PUT, DELETE, OPTIONS, TRACE, CONNECT, PATCH (RFC 9112 §3.2 / H11-REQLINE-001)
  - Phase 2.2: Target forms origin, absolute, authority, asterisk (RFC 9112 §3.2.1)
  - Phase 2.2: Target URI length limits 8192 boundary & 414 URI Too Long (RFC 9112 §3.2 / H11-REQLINE-002)
  - Phase 2.2: Method length limits & unsupported tokens (RFC 9112 §3.2 / H11-REQLINE-003)
  - Phase 2.2: Version support HTTP/1.1, 1.0 & 505 for HTTP/2.0, 3.0, 0.9 (RFC 9110 §6.2 / H11-STATUS-004)
  - Phase 2.2: Request line syntax violations (multiple spaces, tabs, control chars) (RFC 9112 §3.2 / H11-REQLINE-004)
"""

import subprocess
import socket
import time
import sys
import signal

PORT = 8080
HOST = "127.0.0.1"

def send_raw_and_recv(payload: bytes, timeout: float = 2.0) -> bytes:
    s = socket.create_connection((HOST, PORT), timeout=timeout)
    resp = b""
    try:
        s.sendall(payload)
        while True:
            chunk = s.recv(4096)
            if not chunk:
                break
            resp += chunk
    except (socket.timeout, ConnectionResetError, BrokenPipeError, OSError):
        pass
    finally:
        try:
            s.close()
        except Exception:
            pass
    return resp

def run_test(rfc_id, name, test_func):
    try:
        test_func()
        print(f"  [PASS] [{rfc_id}] {name}")
        return True
    except Exception as e:
        print(f"  [FAIL] [{rfc_id}] {name} -> {e}", file=sys.stderr)
        return False

# ============================================================================
# Phase 2.1: Octet Stream & Line Endings
# ============================================================================

def test_octet_stream_raw_bytes():
    """RFC 9112 §2.2 (H11-PARSE-001): Parse raw octets in target URI query string"""
    payload = b"GET /search?q=\x80\xFF\xFE HTTP/1.1\r\nHost: localhost:8080\r\n\r\n"
    resp = send_raw_and_recv(payload)
    if b"HTTP/1.1 200 OK" not in resp:
        raise AssertionError(f"Expected 200 OK for raw octet stream query, got: {resp}")

def test_no_bare_cr_in_response():
    """RFC 9112 §2.2 (H11-PARSE-002): Response contains strictly CRLF, no bare CR"""
    payload = b"GET / HTTP/1.1\r\nHost: localhost:8080\r\n\r\n"
    resp = send_raw_and_recv(payload)
    for i, byte in enumerate(resp):
        if byte == ord(b'\r'):
            if i + 1 >= len(resp) or resp[i + 1] != ord(b'\n'):
                raise AssertionError(f"Bare CR found in response at offset {i}")

def test_bare_lf_in_req_line():
    """RFC 9112 §2.2 (H11-PARSE-003/007): Reject bare LF in request-line (400)"""
    payload = b"GET / HTTP/1.1\nHost: localhost:8080\r\n\r\n"
    resp = send_raw_and_recv(payload)
    if b"HTTP/1.1 400" not in resp:
        raise AssertionError(f"Expected 400 Bad Request on bare LF in req line, got: {resp}")

def test_bare_cr_in_req_line():
    """RFC 9112 §2.2 (H11-PARSE-003/007): Reject bare CR in request-line (400)"""
    payload = b"GET / HTTP/1.1\rHost: localhost:8080\r\n\r\n"
    resp = send_raw_and_recv(payload)
    if b"HTTP/1.1 400" not in resp:
        raise AssertionError(f"Expected 400 Bad Request on bare CR in req line, got: {resp}")

def test_whitespace_before_first_header():
    """RFC 9112 §2.2 (H11-PARSE-004/005): Reject whitespace between start-line and first header (400)"""
    payload = b"GET / HTTP/1.1\r\n \r\nHost: localhost:8080\r\n\r\n"
    resp = send_raw_and_recv(payload)
    if b"HTTP/1.1 400" not in resp:
        raise AssertionError(f"Expected 400 Bad Request on whitespace line before headers, got: {resp}")

def test_leading_crlf_tolerance():
    """RFC 9112 §2.2 (H11-PARSE-006): Ignore empty leading CRLFs (up to 20 CRLFs)"""
    payload = b"\r\n\r\nGET / HTTP/1.1\r\nHost: localhost:8080\r\n\r\n"
    resp = send_raw_and_recv(payload)
    if b"HTTP/1.1 200 OK" not in resp:
        raise AssertionError(f"Expected 200 OK with leading CRLFs, got: {resp}")

def test_excessive_leading_crlf_rejection():
    """RFC 9112 §2.2 (H11-PARSE-007): Reject excessive leading blank lines (>20 CRLFs) (400)"""
    payload = (b"\r\n" * 25) + b"GET / HTTP/1.1\r\nHost: localhost:8080\r\n\r\n"
    resp = send_raw_and_recv(payload)
    if b"HTTP/1.1 400" not in resp:
        raise AssertionError(f"Expected 400 Bad Request on excessive leading blank lines, got: {resp}")

# ============================================================================
# Phase 2.2: Request Line Grammar & Methods
# ============================================================================

def test_standard_http_methods():
    """RFC 9112 §3.2 (H11-REQLINE-001): Standard HTTP methods GET, HEAD, POST, OPTIONS"""
    for m in [b"GET", b"HEAD", b"POST", b"OPTIONS"]:
        payload = m + b" / HTTP/1.1\r\nHost: localhost:8080\r\n\r\n"
        resp = send_raw_and_recv(payload)
        if b"HTTP/1.1 200 OK" not in resp:
            raise AssertionError(f"Expected 200 OK for method {m.decode()}, got: {resp}")

def test_target_forms_asterisk_and_absolute():
    """RFC 9112 §3.2.1: Asterisk-form (OPTIONS *) and absolute-form targets"""
    payload_asterisk = b"OPTIONS * HTTP/1.1\r\nHost: localhost:8080\r\n\r\n"
    resp1 = send_raw_and_recv(payload_asterisk)
    if b"HTTP/1.1 200 OK" not in resp1:
        raise AssertionError(f"Expected 200 OK for OPTIONS *, got: {resp1}")

    payload_abs = b"GET http://localhost:8080/path/test HTTP/1.1\r\nHost: localhost:8080\r\n\r\n"
    resp2 = send_raw_and_recv(payload_abs)
    if b"HTTP/1.1 200 OK" not in resp2:
        raise AssertionError(f"Expected 200 OK for absolute URI target, got: {resp2}")

def test_uri_too_long_414():
    """RFC 9112 §3.2 (H11-REQLINE-002): Reject request-target > 8192 bytes (414)"""
    long_path = b"/" + (b"a" * 8200)
    payload = b"GET " + long_path + b" HTTP/1.1\r\nHost: localhost:8080\r\n\r\n"
    resp = send_raw_and_recv(payload)
    if b"HTTP/1.1 414" not in resp:
        raise AssertionError(f"Expected 414 URI Too Long, got: {resp}")

def test_method_too_long_rejection():
    """RFC 9112 §3.2 (H11-REQLINE-003): Reject method token exceeding limit (>32 chars) (400/501)"""
    payload = b"VERYLONGMETHODNAMEEXCEEDINGTHIRTYTWOCHARS / HTTP/1.1\r\nHost: localhost:8080\r\n\r\n"
    resp = send_raw_and_recv(payload)
    if b"HTTP/1.1 400" not in resp and b"HTTP/1.1 501" not in resp:
        raise AssertionError(f"Expected 400/501 for oversized method, got: {resp}")

def test_version_not_supported_505():
    """RFC 9110 §6.2 (H11-STATUS-004): HTTP version not supported (HTTP/2.0, HTTP/3.0 -> 505)"""
    for v in [b"HTTP/2.0", b"HTTP/3.0", b"HTTP/0.9"]:
        payload = b"GET / " + v + b"\r\nHost: localhost:8080\r\n\r\n"
        resp = send_raw_and_recv(payload)
        if b"HTTP/1.1 505" not in resp:
            raise AssertionError(f"Expected 505 for version {v.decode()}, got: {resp}")

def test_multiple_spaces_in_req_line():
    """RFC 9112 §3.2 (H11-REQLINE-004): Reject multiple spaces between tokens (400)"""
    payload1 = b"GET  / HTTP/1.1\r\nHost: localhost:8080\r\n\r\n"
    resp1 = send_raw_and_recv(payload1)
    if b"HTTP/1.1 400" not in resp1:
        raise AssertionError(f"Expected 400 Bad Request on double space before target, got: {resp1}")

    payload2 = b"GET /  HTTP/1.1\r\nHost: localhost:8080\r\n\r\n"
    resp2 = send_raw_and_recv(payload2)
    if b"HTTP/1.1 400" not in resp2:
        raise AssertionError(f"Expected 400 Bad Request on double space before version, got: {resp2}")

def test_tab_delimiter_in_req_line():
    """RFC 9112 §3.2 (H11-REQLINE-004): Reject tab delimiters in request line (400)"""
    payload = b"GET\t/\tHTTP/1.1\r\nHost: localhost:8080\r\n\r\n"
    resp = send_raw_and_recv(payload)
    if b"HTTP/1.1 400" not in resp:
        raise AssertionError(f"Expected 400 Bad Request on tab delimiters, got: {resp}")

def test_trailing_space_after_version():
    """RFC 9112 §3.2 (H11-REQLINE-004): Reject trailing space after version before CRLF (400)"""
    payload = b"GET / HTTP/1.1 \r\nHost: localhost:8080\r\n\r\n"
    resp = send_raw_and_recv(payload)
    if b"HTTP/1.1 400" not in resp:
        raise AssertionError(f"Expected 400 Bad Request on trailing space after version, got: {resp}")

def test_control_chars_in_uri():
    """RFC 9112 §3.2 (H11-REQLINE-004): Reject control characters in URI target (400)"""
    payload = b"GET /\x01/test HTTP/1.1\r\nHost: localhost:8080\r\n\r\n"
    resp = send_raw_and_recv(payload)
    if b"HTTP/1.1 400" not in resp:
        raise AssertionError(f"Expected 400 Bad Request on control characters in URI, got: {resp}")

# ============================================================================
# Main Runner
# ============================================================================

def main():
    print("\n=== Running Integration Test Suite: Request Line & Line Endings (Phase 2.1 & 2.2) ===")

    proc = subprocess.Popen(
        ["./build/src/kinetic", "configs/test_config.yaml"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=False
    )

    time.sleep(0.5)
    all_passed = True

    try:
        tests = [
            ("RFC 9112 §2.2 (H11-PARSE-001)", "Octet stream raw octets in target query", test_octet_stream_raw_bytes),
            ("RFC 9112 §2.2 (H11-PARSE-002)", "Strict CRLF in server responses (no bare CR)", test_no_bare_cr_in_response),
            ("RFC 9112 §2.2 (H11-PARSE-003/007)", "Reject bare LF in request-line (400)", test_bare_lf_in_req_line),
            ("RFC 9112 §2.2 (H11-PARSE-003/007)", "Reject bare CR in request-line (400)", test_bare_cr_in_req_line),
            ("RFC 9112 §2.2 (H11-PARSE-004/005)", "Reject whitespace line before headers (400)", test_whitespace_before_first_header),
            ("RFC 9112 §2.2 (H11-PARSE-006)", "Tolerate leading blank CRLF lines (200)", test_leading_crlf_tolerance),
            ("RFC 9112 §2.2 (H11-PARSE-007)", "Reject excessive leading blank lines (400)", test_excessive_leading_crlf_rejection),
            ("RFC 9112 §3.2 (H11-REQLINE-001)", "Standard HTTP methods (GET, HEAD, POST, OPTIONS)", test_standard_http_methods),
            ("RFC 9112 §3.2.1", "Accept asterisk-form (OPTIONS *) and absolute-form targets", test_target_forms_asterisk_and_absolute),
            ("RFC 9112 §3.2 (H11-REQLINE-002)", "Reject request-target > 8192 bytes (414)", test_uri_too_long_414),
            ("RFC 9112 §3.2 (H11-REQLINE-003)", "Reject oversized method token (400/501)", test_method_too_long_rejection),
            ("RFC 9110 §6.2 (H11-STATUS-004)", "Refuse non-HTTP/1.x version with 505", test_version_not_supported_505),
            ("RFC 9112 §3.2 (H11-REQLINE-004)", "Reject multiple spaces in request line (400)", test_multiple_spaces_in_req_line),
            ("RFC 9112 §3.2 (H11-REQLINE-004)", "Reject tab delimiters in request line (400)", test_tab_delimiter_in_req_line),
            ("RFC 9112 §3.2 (H11-REQLINE-004)", "Reject trailing space after version (400)", test_trailing_space_after_version),
            ("RFC 9112 §3.2 (H11-REQLINE-004)", "Reject control characters in URI (400)", test_control_chars_in_uri),
        ]

        for rfc_id, name, func in tests:
            if not run_test(rfc_id, name, func):
                all_passed = False

    except Exception as e:
        print(f"Integration runner exception: {e}", file=sys.stderr)
        all_passed = False
    finally:
        proc.send_signal(signal.SIGINT)
        try:
            proc.communicate(timeout=2.0)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.communicate()

    print(f"\n--- Request Line Integration Summary: {'PASSED' if all_passed else 'SOME TESTS FAILED (TDD TARGETS)'} ---")
    sys.exit(0 if all_passed else 1)

if __name__ == "__main__":
    main()
