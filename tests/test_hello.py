import subprocess
import socket
import time
import sys
import signal

def main():
    # Start the server with the test config
    proc = subprocess.Popen(
        ["./build/src/kinetic", "configs/test_config.yaml"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True
    )
    
    # Wait a bit for the server to bind and start listening
    time.sleep(0.5)
    
    try:
        # Connect to the echo server on port 8080
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(2.0)
        s.connect(("127.0.0.1", 8080))
        
        # Send data and verify echo
        payload = b"Hello Kinetic Echo Server!"
        s.sendall(payload)
        response = s.recv(len(payload))
        s.close()
        
        print(f"Sent: {payload}")
        print(f"Received: {response}")
        
        if response != payload:
            print("Error: Echo mismatch!", file=sys.stderr)
            proc.terminate()
            sys.exit(1)
            
        print("Echo test successful.")
        
    except Exception as e:
        print(f"Connection/IO failed: {e}", file=sys.stderr)
        proc.terminate()
        sys.exit(1)
    finally:
        # Gracefully terminate the server via SIGINT
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
