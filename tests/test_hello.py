import subprocess
import socket
import time
import sys
import signal

def main():
    proc = subprocess.Popen(
        ["./build/src/kinetic", "configs/test_config.yaml"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )
    
    time.sleep(0.5)
    
    try:
        # Test 1: GET request
        payload_get = b"GET / HTTP/1.1\r\nHost: localhost:8080\r\n\r\n"
        s1 = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s1.settimeout(2.0)
        s1.connect(("127.0.0.1", 8080))
        s1.sendall(payload_get)
        resp1 = s1.recv(1024)
        s1.close()
        
        if b"HTTP/1.1 200 OK" not in resp1:
            print(f"Error: Expected 200 OK for GET. Got: {resp1}", file=sys.stderr)
            proc.terminate()
            sys.exit(1)

        # Test 2: POST request with Content-Length
        payload_post = b"POST /upload HTTP/1.1\r\nHost: localhost:8080\r\nContent-Length: 5\r\n\r\nhello"
        s2 = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s2.settimeout(2.0)
        s2.connect(("127.0.0.1", 8080))
        s2.sendall(payload_post)
        resp2 = s2.recv(1024)
        s2.close()

        if b"HTTP/1.1 200 OK" not in resp2:
            print(f"Error: Expected 200 OK for POST. Got: {resp2}", file=sys.stderr)
            proc.terminate()
            sys.exit(1)

        # Test 3: POST request with Transfer-Encoding: chunked
        payload_chunked = (
            b"POST /upload HTTP/1.1\r\nHost: localhost:8080\r\nTransfer-Encoding: chunked\r\n\r\n"
            b"4\r\nWiki\r\n6\r\npedia \r\n0\r\n\r\n"
        )
        s3 = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s3.settimeout(2.0)
        s3.connect(("127.0.0.1", 8080))
        s3.sendall(payload_chunked)
        resp3 = s3.recv(1024)
        s3.close()

        if b"HTTP/1.1 200 OK" not in resp3:
            print(f"Error: Expected 200 OK for chunked POST. Got: {resp3}", file=sys.stderr)
            proc.terminate()
            sys.exit(1)

        # Test 4: Smuggling mitigation test (both CL and TE present)
        payload_smuggle = b"POST / HTTP/1.1\r\nHost: localhost:8080\r\nContent-Length: 5\r\nTransfer-Encoding: chunked\r\n\r\n"
        s4 = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s4.settimeout(2.0)
        s4.connect(("127.0.0.1", 8080))
        s4.sendall(payload_smuggle)
        resp4 = s4.recv(1024)
        s4.close()

        if b"HTTP/1.1 400" not in resp4:
            print(f"Error: Expected 400 Bad Request for smuggling payload. Got: {resp4}", file=sys.stderr)
            proc.terminate()
            sys.exit(1)
            
        print("All sequential E2E integration tests successful.")
        
    except Exception as e:
        print(f"Connection/IO failed: {e}", file=sys.stderr)
        proc.terminate()
        sys.exit(1)
    finally:
        print("Sending SIGINT to server...")
        proc.send_signal(signal.SIGINT)
        try:
            stdout, stderr = proc.communicate(timeout=2.0)
            print("Server stdout:")
            print(stdout)
            print("Server stderr:")
            print(stderr)
        except subprocess.TimeoutExpired:
            print("Force terminating server...")
            proc.kill()
            proc.communicate()
            sys.exit(1)
            
    if proc.returncode != 0:
        print(f"Server exited with non-zero status: {proc.returncode}", file=sys.stderr)
        sys.exit(1)
        
    print("Integration test passed successfully.")
    sys.exit(0)

if __name__ == "__main__":
    main()
