#!/usr/bin/env python3
import subprocess
import socket
import time
import sys
import signal

def run_test(rfc_id, name, test_func):
    try:
        test_func()
        print(f"  [PASS] [{rfc_id}] {name}")
        return True
    except Exception as e:
        print(f"  [FAIL] [{rfc_id}] {name} -> {e}", file=sys.stderr)
        return False

def test_get_simple():
    payload = b"GET / HTTP/1.1\r\nHost: localhost:8080\r\n\r\n"
    s = socket.create_connection(("127.0.0.1", 8080), timeout=2.0)
    s.sendall(payload)
    resp = s.recv(1024)
    s.close()
    if b"HTTP/1.1 200 OK" not in resp:
        raise AssertionError(f"Expected 200 OK, got: {resp}")

def test_post_content_length():
    payload = b"POST /upload HTTP/1.1\r\nHost: localhost:8080\r\nContent-Length: 5\r\n\r\nhello"
    s = socket.create_connection(("127.0.0.1", 8080), timeout=2.0)
    s.sendall(payload)
    resp = s.recv(1024)
    s.close()
    if b"HTTP/1.1 200 OK" not in resp:
        raise AssertionError(f"Expected 200 OK, got: {resp}")

def test_post_chunked():
    payload = (
        b"POST /upload HTTP/1.1\r\nHost: localhost:8080\r\nTransfer-Encoding: chunked\r\n\r\n"
        b"4\r\nWiki\r\n6\r\npedia \r\n0\r\n\r\n"
    )
    s = socket.create_connection(("127.0.0.1", 8080), timeout=2.0)
    s.sendall(payload)
    resp = s.recv(1024)
    s.close()
    if b"HTTP/1.1 200 OK" not in resp:
        raise AssertionError(f"Expected 200 OK, got: {resp}")

def test_smuggling_rejection():
    payload = b"POST / HTTP/1.1\r\nHost: localhost:8080\r\nContent-Length: 5\r\nTransfer-Encoding: chunked\r\n\r\n"
    s = socket.create_connection(("127.0.0.1", 8080), timeout=2.0)
    s.sendall(payload)
    resp = s.recv(1024)
    s.close()
    if b"HTTP/1.1 400" not in resp:
        raise AssertionError(f"Expected 400 Bad Request, got: {resp}")

def test_staged_shutdown_eof():
    payload = b"GET / HTTP/1.1\r\nHost: localhost:8080\r\n\r\n"
    s = socket.create_connection(("127.0.0.1", 8080), timeout=2.0)
    s.sendall(payload)
    resp = s.recv(1024)
    if not resp:
        raise AssertionError("Empty initial response from server")
    eof = s.recv(1024)
    s.close()
    if eof != b"":
        raise AssertionError(f"Expected EOF on socket, got extra bytes: {eof}")

def main():
    print("\n=== Running Integration Test Suite: Connection Handling ===")
    
    proc = subprocess.Popen(
        ["./build/src/kinetic", "configs/test_config.yaml"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )
    
    time.sleep(0.5)
    passed = True
    
    try:
        tests = [
            ("RFC 9112 §3.2 (H11-REQLINE-001)", "GET / -> 200 OK Response", test_get_simple),
            ("RFC 9112 §6.3 (H11-FRAME-008)", "POST with Content-Length -> 200 OK", test_post_content_length),
            ("RFC 9112 §7.1 (H11-CHUNK-001)", "POST with Transfer-Encoding: chunked -> 200 OK", test_post_chunked),
            ("RFC 9112 §6.3 / §11.2 (H11-SEC-003)", "Smuggling payload (CL + TE) -> 400 Bad Request", test_smuggling_rejection),
            ("RFC 9112 §9.6 (H11-LIFE-006)", "Staged shutdown delivers response and closes socket with EOF", test_staged_shutdown_eof),
        ]
        
        for rfc_id, name, func in tests:
            if not run_test(rfc_id, name, func):
                passed = False
                
    except Exception as e:
        print(f"Integration runner exception: {e}", file=sys.stderr)
        passed = False
    finally:
        proc.send_signal(signal.SIGINT)
        try:
            proc.communicate(timeout=2.0)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.communicate()

    print(f"\n--- Integration Test Summary: {'PASSED' if passed else 'FAILED'} ---")
    sys.exit(0 if passed else 1)

if __name__ == "__main__":
    main()
