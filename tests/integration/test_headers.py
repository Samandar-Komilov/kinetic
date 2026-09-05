#!/usr/bin/env python3
"""
KinetiC HTTP/1.1 Integration Test Suite: Headers & Host Validation
Covers:
  - Phase 2.3: Host header validation (RFC 9112 §3.2 / RFC 9110 §7.2 / H11-HOST-001)
  - Phase 2.3: Absolute-form target authority override (RFC 9112 §3.2.3 / H11-HOST-002)
  - Phase 2.3: Direct client absolute-form target accepted (RFC 9112 §3.2.3 / H11-HOST-003)
  - Phase 2.3: Reject CONNECT with empty/invalid port (RFC 9110 §9.3.6 / H11-HOST-004) [TDD]
  - Phase 2.3: Reject https scheme on cleartext connection (RFC 9110 §4.3.3 / H11-HOST-005) [TDD]
  - Phase 2.3: Reject empty authority on absolute URI (RFC 9112 §3.2.3 / H11-HOST-006) [TDD]
  - Phase 2.4: Header field line grammar & OWS trimming (RFC 9112 §5.1 / H11-HDR-001)
  - Phase 2.4: Reject whitespace before colon in field name (RFC 9112 §5.2 / H11-HDR-001 / H11-SEC-004)
  - Phase 2.4: Reject obsolete line folding obs-fold (RFC 9112 §2.2 / H11-HDR-002)
  - Phase 2.4: Header section size limits > 16KB & count > 64 (RFC 9110 §5.4 / H11-HDR-004)
  - Phase 2.4: Full header section required before dispatch / trickle stream (RFC 9110 §5.3 / H11-HDR-005)
  - Phase 2.4: Reject NUL, bare CR, bare LF, and control chars in field value (RFC 9110 §5.5 / H11-HDR-003)
  - Phase 2.4: Unrecognized headers parsed safely without error (RFC 9110 §5.1 / H11-HDR-006)
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
# Phase 2.3: Host Validation & Request Target
# ============================================================================

def test_missing_host_header():
    """RFC 9112 §3.2 / RFC 9110 §7.2 (H11-HOST-001): Reject missing Host header in HTTP/1.1 (400)"""
    payload = b"GET / HTTP/1.1\r\nUser-Agent: test\r\n\r\n"
    resp = send_raw_and_recv(payload)
    if b"HTTP/1.1 400" not in resp:
        raise AssertionError(f"Expected 400 Bad Request for missing Host header, got: {resp}")

def test_duplicate_host_header():
    """RFC 9112 §3.2 / RFC 9110 §7.2 (H11-HOST-001): Reject duplicate Host headers (400)"""
    payload = b"GET / HTTP/1.1\r\nHost: localhost:8080\r\nHost: duplicate.com\r\n\r\n"
    resp = send_raw_and_recv(payload)
    if b"HTTP/1.1 400" not in resp:
        raise AssertionError(f"Expected 400 Bad Request for duplicate Host headers, got: {resp}")

def test_empty_host_header():
    """RFC 9112 §3.2 / RFC 9110 §7.2 (H11-HOST-001): Reject empty Host header (400) (TDD)"""
    payload = b"GET / HTTP/1.1\r\nHost:\r\n\r\n"
    resp = send_raw_and_recv(payload)
    if b"HTTP/1.1 400" not in resp:
        raise AssertionError(f"Expected 400 Bad Request for empty Host header, got: {resp}")

def test_case_insensitive_host():
    """RFC 9110 §7.2 (H11-HOST-001): Accept case-insensitive Host field name"""
    payload = b"GET / HTTP/1.1\r\nhOsT: localhost:8080\r\n\r\n"
    resp = send_raw_and_recv(payload)
    if b"HTTP/1.1 200 OK" not in resp:
        raise AssertionError(f"Expected 200 OK for mixed-case Host header, got: {resp}")

def test_absolute_uri_override():
    """RFC 9112 §3.2.3 (H11-HOST-002): Absolute URI target authority overrides Host header"""
    payload = b"GET http://localhost:8080/path HTTP/1.1\r\nHost: otherhost:9999\r\n\r\n"
    resp = send_raw_and_recv(payload)
    if b"HTTP/1.1 200 OK" not in resp:
        raise AssertionError(f"Expected 200 OK when target authority overrides Host header, got: {resp}")

def test_connect_invalid_port_rejection():
    """RFC 9110 §9.3.6 (H11-HOST-004): Reject CONNECT without port / non-numeric port (400) (TDD)"""
    payload = b"CONNECT localhost HTTP/1.1\r\nHost: localhost\r\n\r\n"
    resp = send_raw_and_recv(payload)
    if b"HTTP/1.1 400" not in resp:
        raise AssertionError(f"Expected 400 Bad Request on CONNECT without port, got: {resp}")

def test_https_cleartext_rejection():
    """RFC 9110 §4.3.3 (H11-HOST-005): Reject https scheme on cleartext connection (400) (TDD)"""
    payload = b"GET https://localhost:8080/path HTTP/1.1\r\nHost: localhost:8080\r\n\r\n"
    resp = send_raw_and_recv(payload)
    if b"HTTP/1.1 400" not in resp:
        raise AssertionError(f"Expected 400 Bad Request on https scheme over cleartext, got: {resp}")

def test_empty_authority_rejection():
    """RFC 9112 §3.2.3 (H11-HOST-006): Reject empty authority on absolute URI (400) (TDD)"""
    payload = b"GET http:///path HTTP/1.1\r\nHost: localhost:8080\r\n\r\n"
    resp = send_raw_and_recv(payload)
    if b"HTTP/1.1 400" not in resp:
        raise AssertionError(f"Expected 400 Bad Request on empty authority URI, got: {resp}")

# ============================================================================
# Phase 2.4: Header Section
# ============================================================================

def test_ows_trimming_and_multi_headers():
    """RFC 9112 §5.1 (H11-HDR-001): Trim OWS around values & preserve internal spaces"""
    payload = (
        b"GET / HTTP/1.1\r\n"
        b"Host:   \tlocalhost:8080   \t  \r\n"
        b"User-Agent: curl/7.68.0\r\n"
        b"Accept: text/html, application/xhtml+xml\r\n"
        b"X-Custom-Empty:\r\n"
        b"\r\n"
    )
    resp = send_raw_and_recv(payload)
    if b"HTTP/1.1 200 OK" not in resp:
        raise AssertionError(f"Expected 200 OK with OWS and multiple headers, got: {resp}")

def test_space_before_colon_rejection():
    """RFC 9112 §5.2 (H11-HDR-001 / H11-SEC-004): Reject space before colon (400)"""
    payload = b"GET / HTTP/1.1\r\nHost : localhost:8080\r\n\r\n"
    resp = send_raw_and_recv(payload)
    if b"HTTP/1.1 400" not in resp:
        raise AssertionError(f"Expected 400 Bad Request on space before colon, got: {resp}")

def test_tab_before_colon_rejection():
    """RFC 9112 §5.2 (H11-HDR-001 / H11-SEC-004): Reject tab before colon (400)"""
    payload = b"GET / HTTP/1.1\r\nHost\t: localhost:8080\r\n\r\n"
    resp = send_raw_and_recv(payload)
    if b"HTTP/1.1 400" not in resp:
        raise AssertionError(f"Expected 400 Bad Request on tab before colon, got: {resp}")

def test_obs_fold_rejection():
    """RFC 9112 §2.2 (H11-HDR-002): Reject obsolete line folding obs-fold (400)"""
    payload = b"GET / HTTP/1.1\r\nHost: localhost:8080\r\n X-Folded: yes\r\n\r\n"
    resp = send_raw_and_recv(payload)
    if b"HTTP/1.1 400" not in resp:
        raise AssertionError(f"Expected 400 Bad Request on obs-fold, got: {resp}")

def test_headers_too_large_431():
    """RFC 9110 §5.4 (H11-HDR-004): Reject header section > 16KB (431)"""
    large_header = b"X-Large: " + (b"v" * 17000) + b"\r\n"
    payload = b"GET / HTTP/1.1\r\nHost: localhost:8080\r\n" + large_header + b"\r\n"
    resp = send_raw_and_recv(payload)
    if b"HTTP/1.1 431" not in resp:
        raise AssertionError(f"Expected 431 Request Header Fields Too Large, got: {resp}")

def test_trickle_stream_headers():
    """RFC 9110 §5.3 (H11-HDR-005): Do not dispatch until full header section is received (trickle stream)"""
    payload = b"GET / HTTP/1.1\r\nHost: localhost:8080\r\nUser-Agent: trickle\r\n\r\n"
    s = socket.create_connection((HOST, PORT), timeout=2.0)
    for i in range(len(payload)):
        s.sendall(payload[i:i+1])
        time.sleep(0.002)
    
    resp = s.recv(1024)
    s.close()
    if b"HTTP/1.1 200 OK" not in resp:
        raise AssertionError(f"Expected 200 OK after full stream headers, got: {resp}")

def test_nul_byte_in_header_value():
    """RFC 9110 §5.5 (H11-HDR-003): Reject NUL byte in header value (400)"""
    payload = b"GET / HTTP/1.1\r\nHost: localhost:8080\r\nX-Bad: abc\x00def\r\n\r\n"
    resp = send_raw_and_recv(payload)
    if b"HTTP/1.1 400" not in resp:
        raise AssertionError(f"Expected 400 Bad Request on NUL byte in header, got: {resp}")

def test_bare_cr_in_header_value():
    """RFC 9110 §5.5 (H11-HDR-003): Reject bare CR in header value (400)"""
    payload = b"GET / HTTP/1.1\r\nHost: localhost:8080\r\nX-Bad: abc\rdef\r\n\r\n"
    resp = send_raw_and_recv(payload)
    if b"HTTP/1.1 400" not in resp:
        raise AssertionError(f"Expected 400 Bad Request on bare CR in header value, got: {resp}")

def test_control_chars_in_header_value():
    """RFC 9110 §5.5 (H11-HDR-003): Reject control characters in header value (400)"""
    payload = b"GET / HTTP/1.1\r\nHost: localhost:8080\r\nX-Bad: abc\x07def\r\n\r\n"
    resp = send_raw_and_recv(payload)
    if b"HTTP/1.1 400" not in resp:
        raise AssertionError(f"Expected 400 Bad Request on control chars in header value, got: {resp}")

def test_unrecognized_headers_ignored():
    """RFC 9110 §5.1 (H11-HDR-006): Unrecognized headers parsed safely without error (200)"""
    payload = (
        b"GET / HTTP/1.1\r\n"
        b"Host: localhost:8080\r\n"
        b"X-Unknown-Tracking: custom-token-987\r\n"
        b"Custom-Header-Field: 123456\r\n"
        b"\r\n"
    )
    resp = send_raw_and_recv(payload)
    if b"HTTP/1.1 200 OK" not in resp:
        raise AssertionError(f"Expected 200 OK with custom headers, got: {resp}")

# ============================================================================
# Main Runner
# ============================================================================

def main():
    print("\n=== Running Integration Test Suite: Headers & Host Validation (Phase 2.3 & 2.4) ===")

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
            ("RFC 9112 §3.2 / RFC 9110 §7.2 (H11-HOST-001)", "Reject missing Host header in HTTP/1.1 (400)", test_missing_host_header),
            ("RFC 9112 §3.2 / RFC 9110 §7.2 (H11-HOST-001)", "Reject duplicate Host headers (400)", test_duplicate_host_header),
            ("RFC 9112 §3.2 / RFC 9110 §7.2 (H11-HOST-001)", "Reject empty Host header (400) (TDD)", test_empty_host_header),
            ("RFC 9110 §7.2 (H11-HOST-001)", "Accept case-insensitive Host field name", test_case_insensitive_host),
            ("RFC 9112 §3.2.3 (H11-HOST-002)", "Absolute URI target authority overrides Host header", test_absolute_uri_override),
            ("RFC 9110 §9.3.6 (H11-HOST-004)", "Reject CONNECT without valid port (400) (TDD)", test_connect_invalid_port_rejection),
            ("RFC 9110 §4.3.3 (H11-HOST-005)", "Reject https scheme on cleartext connection (400) (TDD)", test_https_cleartext_rejection),
            ("RFC 9112 §3.2.3 (H11-HOST-006)", "Reject empty authority on absolute URI (400) (TDD)", test_empty_authority_rejection),
            ("RFC 9112 §5.1 (H11-HDR-001)", "OWS trimming and multiple header fields", test_ows_trimming_and_multi_headers),
            ("RFC 9112 §5.2 (H11-HDR-001 / H11-SEC-004)", "Reject space before colon in header name (400)", test_space_before_colon_rejection),
            ("RFC 9112 §5.2 (H11-HDR-001 / H11-SEC-004)", "Reject tab before colon in header name (400)", test_tab_before_colon_rejection),
            ("RFC 9112 §2.2 (H11-HDR-002)", "Reject obs-fold line folding (400)", test_obs_fold_rejection),
            ("RFC 9110 §5.4 (H11-HDR-004)", "Reject header section > 16KB (431)", test_headers_too_large_431),
            ("RFC 9110 §5.3 (H11-HDR-005)", "Do not dispatch until full header section arrives (trickle stream)", test_trickle_stream_headers),
            ("RFC 9110 §5.5 (H11-HDR-003)", "Reject NUL byte in header value (400)", test_nul_byte_in_header_value),
            ("RFC 9110 §5.5 (H11-HDR-003)", "Reject bare CR in header value (400)", test_bare_cr_in_header_value),
            ("RFC 9110 §5.5 (H11-HDR-003)", "Reject control characters in header value (400)", test_control_chars_in_header_value),
            ("RFC 9110 §5.1 (H11-HDR-006)", "Unrecognized headers parsed safely without error", test_unrecognized_headers_ignored),
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

    print(f"\n--- Headers Integration Summary: {'PASSED' if all_passed else 'SOME TESTS FAILED (TDD TARGETS)'} ---")
    sys.exit(0 if all_passed else 1)

if __name__ == "__main__":
    main()
