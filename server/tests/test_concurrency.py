import socket
import threading

# Configuration
SERVER_HOST = "127.0.0.1"  # Server IP address
SERVER_PORT = 8080         # Server port
NUM_CLIENTS = 100       # Number of concurrent clients

def emulate_client(client_id):
    try:
        # Connect to the server
        client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        client_socket.connect((SERVER_HOST, SERVER_PORT))
        print(f"Client {client_id}: Connected to the server.")
        
        # Receive the welcome message
        message = client_socket.recv(1024).decode()
        print(f"Client {client_id}: Received message: {message}")
        
        # Send a dummy command
        client_socket.sendall(f"Hello from client {client_id}\n".encode())
        
        # Receive response (if any)
        response = client_socket.recv(1024).decode()
        print(f"Client {client_id}: Server response: {response}")
        
    except Exception as e:
        print(f"Client {client_id}: Error - {e}")
    finally:
        client_socket.close()
        print(f"Client {client_id}: Connection closed.")

# Launch multiple client threads
threads = []
for i in range(NUM_CLIENTS):
    thread = threading.Thread(target=emulate_client, args=(i,))
    threads.append(thread)
    thread.start()

# Wait for all threads to complete
for thread in threads:
    thread.join()

print("All clients finished.")
