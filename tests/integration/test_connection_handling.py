#!/usr/bin/env python3
"""
KinetiC HTTP/1.1 Integration Test Suite: Connection Management & Socket Lifecycle
Covers:
  - Phase 1.1: Transport & Socket Accept Setup (Pool slot allocation, concurrent clients)
  - Phase 1.1: RFC 9112 §9.6 (H11-LIFE-006): Staged graceful socket shutdown & half-close
  - Phase 1.1: RFC 9112 §9.3 (H11-CONN-002): Send Connection: close after single cycle
  - Phase 2.5: Body framing, Content-Length, Chunked encoding, and Smuggling guards
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
# Phase 1.1: Connection Management & Socket Lifecycle Tests
# ============================================================================

def test_connection_pool_concurrency():
    """Fixed-size connection pool slot allocator: concurrent clients"""
    sockets = []
    try:
        for _ in range(10):
            s = socket.create_connection((HOST, PORT), timeout=2.0)
            sockets.append(s)
        
        for s in sockets:
            s.sendall(b"GET / HTTP/1.1\r\nHost: localhost:8080\r\n\r\n")
            resp = s.recv(1024)
            if b"HTTP/1.1 200 OK" not in resp:
                raise AssertionError(f"Expected 200 OK on concurrent connection, got: {resp}")
    finally:
        for s in sockets:
            try:
                s.close()
            except Exception:
                pass

def test_staged_shutdown_half_close():
    """RFC 9112 §9.6 (H11-LIFE-006): Client half-close write delivers response & closes with EOF"""
    payload = b"GET / HTTP/1.1\r\nHost: localhost:8080\r\n\r\n"
    s = socket.create_connection((HOST, PORT), timeout=2.0)
    s.sendall(payload)
    s.shutdown(socket.SHUT_WR)
    
    resp = s.recv(1024)
    if not resp or b"HTTP/1.1 200 OK" not in resp:
        s.close()
        raise AssertionError(f"Expected 200 OK after half-close, got: {resp}")
    
    eof = s.recv(1024)
    s.close()
    if eof != b"":
        raise AssertionError(f"Expected socket EOF after server response, got: {eof}")

def test_connection_close_header():
    """RFC 9112 §9.3 (H11-CONN-002): Response contains Connection: close"""
    payload = b"GET / HTTP/1.1\r\nHost: localhost:8080\r\n\r\n"
    resp = send_raw_and_recv(payload)
    if b"Connection: close" not in resp:
        raise AssertionError(f"Expected 'Connection: close' header, got: {resp}")

def test_single_cycle_closes_socket():
    """RFC 9112 §9.3 (H11-CONN-002): Socket closed after response in pre-persistence mode"""
    s = socket.create_connection((HOST, PORT), timeout=2.0)
    s.sendall(b"GET / HTTP/1.1\r\nHost: localhost:8080\r\n\r\n")
    resp = s.recv(1024)
    if b"HTTP/1.1 200 OK" not in resp:
        s.close()
        raise AssertionError(f"Expected 200 OK, got: {resp}")
    
    # Server should close socket after response
    second_read = s.recv(1024)
    s.close()
    if second_read != b"":
        raise AssertionError(f"Expected EOF after single cycle response, got: {second_read}")

# ============================================================================
# Phase 2.5: Body Framing & Smuggling Guards
# ============================================================================

def test_post_content_length():
    """RFC 9112 §6.3 (H11-FRAME-008): POST with Content-Length body payload"""
    payload = b"POST /upload HTTP/1.1\r\nHost: localhost:8080\r\nContent-Length: 5\r\n\r\nhello"
    resp = send_raw_and_recv(payload)
    if b"HTTP/1.1 200 OK" not in resp:
        raise AssertionError(f"Expected 200 OK, got: {resp}")

def test_post_chunked():
    """RFC 9112 §7.1 (H11-CHUNK-001): POST with chunked transfer coding"""
    payload = (
        b"POST /upload HTTP/1.1\r\nHost: localhost:8080\r\nTransfer-Encoding: chunked\r\n\r\n"
        b"4\r\nWiki\r\n6\r\npedia \r\n0\r\n\r\n"
    )
    resp = send_raw_and_recv(payload)
    if b"HTTP/1.1 200 OK" not in resp:
        raise AssertionError(f"Expected 200 OK, got: {resp}")

def test_smuggling_cl_and_te():
    """RFC 9112 §6.3 / §11.2 (H11-SEC-003): Smuggling payload (CL + TE) -> 400 Bad Request"""
    payload = b"POST / HTTP/1.1\r\nHost: localhost:8080\r\nContent-Length: 5\r\nTransfer-Encoding: chunked\r\n\r\n"
    resp = send_raw_and_recv(payload)
    if b"HTTP/1.1 400" not in resp:
        raise AssertionError(f"Expected 400 Bad Request for CL+TE smuggling payload, got: {resp}")

# ============================================================================
# Main Runner
# ============================================================================

def main():
    print("\n=== Running Integration Test Suite: Connection Handling (Phase 1.1) ===")

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
            ("RFC 9112 §9.6 (Pool Allocator)", "Concurrent connection slots allocation & reuse", test_connection_pool_concurrency),
            ("RFC 9112 §9.6 (H11-LIFE-006)", "Staged shutdown half-close delivered with EOF", test_staged_shutdown_half_close),
            ("RFC 9112 §9.3 (H11-CONN-002)", "Response includes Connection: close header", test_connection_close_header),
            ("RFC 9112 §9.3 (H11-CONN-002)", "Socket closes after single request/response cycle", test_single_cycle_closes_socket),
            ("RFC 9112 §6.3 (H11-FRAME-008)", "POST with Content-Length -> 200 OK", test_post_content_length),
            ("RFC 9112 §7.1 (H11-CHUNK-001)", "POST with Transfer-Encoding: chunked -> 200 OK", test_post_chunked),
            ("RFC 9112 §6.3 / §11.2 (H11-SEC-003)", "Smuggling payload (CL + TE) -> 400 Bad Request", test_smuggling_cl_and_te),
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

    print(f"\n--- Integration Connection Handling Summary: {'PASSED' if all_passed else 'FAILED'} ---")
    sys.exit(0 if all_passed else 1)

if __name__ == "__main__":
    main()
