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
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(2.0)
        s.connect(("127.0.0.1", 8080))
        
        # Send HTTP request-line and verify response
        payload = b"GET / HTTP/1.1\r\nHost: localhost:8080\r\n\r\n"
        s.sendall(payload)
        response = s.recv(1024)
        s.close()
        
        print(f"Sent: {payload}")
        print(f"Received: {response}")
        
        if b"HTTP/1.1 200 OK" not in response:
            print("Error: Expected 200 OK!", file=sys.stderr)
            proc.terminate()
            sys.exit(1)
            
        print("HTTP parsing and response test successful.")
        
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
