import socket

def test_put():
    """Simulate the `put` command to upload a file to the server."""
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
            # Connect to the server
            sock.connect(("127.0.0.1", 8080))
            print("Connected to server for `put` test.")

            response = sock.recv(1024).decode()
            print(f"Server Response: {response}")
            
            # Send `put` command
            sock.sendall(b"put test_put.txt\n")
            print("Sent `put` command, waiting for server acknowledgment...")

            # Wait for the server's READY_TO_RECEIVE acknowledgment
            response = sock.recv(1024).decode()
            print(f"Server Response: {response}")

            if "READY_TO_RECEIVE" in response:
                # Send file contents
                print("Sending file content...")
                file_content = "This is a test file.\nIt has multiple lines.\n"
                sock.sendall(file_content.encode())

                # Signal end of transfer
                print("Sending end of file transfer signal...")
                sock.sendall(b"FILE_TRANSFER_END\n")

                # Wait for server confirmation
                response = sock.recv(1024).decode()
                print(f"Server Response: {response}")
            else:
                print("Server did not acknowledge `put` command.")
    except Exception as e:
        print(f"An error occurred: {e}")

# Run the `put` test
test_put()
