#include <iostream>
#include <unistd.h>
#include <stdexcept>
#include <fstream>
#include <arpa/inet.h>
#include <cstring>
#include <sys/socket.h>
#include <netdb.h>


#define BUFFER_SIZE 1024


/**
 * @brief Receives a response message from a socket.
 * 
 * This function reads data from the given socket and returns it as a `std::string`.
 * It handles the null-termination of the received data to ensure the response is properly formatted.
 * 
 * @param sock The socket file descriptor from which to receive the message.
 * 
 * @return A `std::string` containing the message received from the socket.
 * 
 * @throws std::runtime_error If the connection is closed or if no data is received.
 * 
 * @note Ensure that the socket is properly connected and initialized before calling this function.
 */
std::string receive_response(int sock) {
    char message_buffer[BUFFER_SIZE];
    ssize_t bytes_received = recv(sock, message_buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received <= 0) {
        throw std::runtime_error("Disconnected from server.");
    }

    message_buffer[bytes_received] = '\0';
    return std::string(message_buffer);
}


/**
 * @brief Sends a standardized response to the client.
 * 
 * This function formats the response as "<message>\n" and sends it to the client.
 * It is useful for maintaining consistency in server-client communication.
 * 
 * @param sock The client's socket file descriptor.
 * @param command A command to be issued to the server.
 */ 
void send_command(int sock, const std::string &command) {
    send(sock, command.c_str(), command.size(), 0);
}


/**
 * @brief Handles the "get" command to download a file from the server.
 * 
 * This function sends a "get <filename>" command to the server, initiates a file transfer,
 * and writes the received data to a local file. It ensures proper handling of the server's
 * response and detects the end of the file transfer using a "FILE_TRANSFER_END" marker.
 * 
 * @param sock The socket file descriptor used for communication with the server.
 * @param filename The name of the file to be downloaded from the server.
 * 
 * @note The function creates a local file with the same name as the requested file.
 *       If the file already exists locally, it will be overwritten.
 * 
 * @warning Ensure the server adheres to the "FILE_TRANSFER_START" and "FILE_TRANSFER_END"
 *          protocol for this function to work correctly.
 * 
 * @details
 * - Sends the command "get <filename>" to the server.
 * - Receives a response from the server to confirm file transfer readiness.
 * - Streams file data from the server until the "FILE_TRANSFER_END" marker is found.
 * - Writes the file data to a binary file with the given filename.
 * - Handles errors such as connection issues or inability to create the local file.
 * 
 * @throws std::runtime_error If there are socket-related issues during communication.
 * 
 * @example
 * @code
 * int sock = connect_to_server();
 * handle_get(sock, "example.txt");
 * @endcode
 */
void handle_get(int sock, const std::string &filename) {
    send_command(sock, "get " + filename);
    std::string response = receive_response(sock);

    if (response.find("SUCCESS: FILE_TRANSFER_START") == 0) {
        std::ofstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Error: Unable to create local file.\n";
            return;
        }

        char buffer[BUFFER_SIZE];
        while (true) {
            ssize_t bytes_received = recv(sock, buffer, BUFFER_SIZE, 0);
            if (bytes_received <= 0) {
                break;
            }

            std::string chunk(buffer, bytes_received);
            size_t end_position = chunk.find("FILE_TRANSFER_END\n");
            if (end_position != std::string::npos) {
                file.write(chunk.c_str(), end_position);
                break;
            }

            file.write(buffer, bytes_received);
        }

        file.close();
        std::cout << "File received successfully: " << filename << "\n";
    } else {
        std::cerr << response << "\n";
    }
}


/**
 * @brief Handles the "put" command to upload a file to the server.
 * 
 * This function opens a local file, sends a "put <filename>" command to the server, 
 * and transmits the file's content. It ensures the server is ready to receive data 
 * and appends a "FILE_TRANSFER_END" marker to signal the end of the file transfer.
 * 
 * @param sock The socket file descriptor used for communication with the server.
 * @param filename The name of the file to be uploaded to the server.
 * 
 * @note The function expects the server to respond with "SUCCESS: READY_TO_RECEIVE" 
 *       before transmitting the file. If the file does not exist locally or the server 
 *       is not ready, the operation will terminate.
 * 
 * @details
 * - Opens the specified file in binary mode for reading.
 * - Sends the command "put <filename>" to inform the server of the upload request.
 * - Waits for a confirmation response from the server before proceeding.
 * - Reads the file in chunks (using a buffer) and sends each chunk over the socket.
 * - Sends a "FILE_TRANSFER_END" marker to signify the end of the file transfer.
 * - Handles server responses after the transfer to confirm the operation's success.
 * 
 * @warning Ensure that the server implements the "READY_TO_RECEIVE" and "FILE_TRANSFER_END" 
 *          protocol for successful operation.
 * 
 * @throws std::runtime_error If there are socket-related issues during communication.
 * 
 * @example
 * @code
 * int sock = connect_to_server();
 * handle_put(sock, "example.txt");
 * @endcode
 */
void handle_put(int sock, const std::string &filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Error: Unable to open file.\n";
        return;
    }

    send_command(sock, "put " + filename);
    std::string response = receive_response(sock);
    
    if (response.find("SUCCESS: READY_TO_RECEIVE") == 0) {
        std:: cout << "Transmitting File\n";
        
        char buffer[BUFFER_SIZE];
        while (file.read(buffer, sizeof(buffer))) {
            send(sock, buffer, file.gcount(), 0);
        }
        if (file.gcount() > 0) {
            send(sock, buffer, file.gcount(), 0);
        }
         
        std::string end_message = "FILE_TRANSFER_END\n";
        send(sock, end_message.c_str(), end_message.size(), 0);
        std::cout << "You sent a file: " << filename << "\n";

        response = receive_response(sock);
        std::cout << response << "\n"; 
    } else {
        std::cerr << response << "Server not ready to Receive" << "\n";
    }
}


/**
 * @brief Handles the main interactive client loop.
 * 
 * Continuously reads user commands, sends them to the server, and processes responses.
 * Supports file upload ("put"), file download ("get"), and termination ("quit").
 * 
 * @param sock The socket file descriptor for communication with the server.
 */
void client_loop(int sock) {
    std::string command;
    std::string response = receive_response(sock);
    std::cout << response;

    while (true) {
        std::cout << "myftp>";
        std::getline(std::cin, command);

        if (command.empty()){
            continue;
        }

        if (command.compare("quit") == 0) {
            send_command(sock, "quit");
            break;
        }

        if (command.substr(0, 4) == "put ") {
            std::string filename = command.substr(4);
            // TODO handle put
            handle_put(sock, filename);

        } else if (command.substr(0, 4) == "get ") {
            std::string filename = command.substr(4);
            // TODO Handle get
            handle_get(sock, filename);
        } else {
            send_command(sock, command);
            std::string response = receive_response(sock);
            std::cout << response;
        }
    }
}


/**
 * @brief Establishes a connection to the server.
 * 
 * Resolves the hostname and connects to the specified server using the given port.
 * 
 * @param hostname The server hostname or IP address.
 * @param port The port number to connect to.
 * @param sock Reference to a socket file descriptor that will be initialized upon successful connection.
 * 
 * @throws std::runtime_error If unable to resolve the hostname or connect to the server.
 */
void connect_to_server(const std::string &hostname, int port, int &sock) {
    struct addrinfo hints, *res, *p;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    std::string port_str = std::to_string(port);

    int status = getaddrinfo(hostname.c_str(), port_str.c_str(), &hints, &res);
    if (status != 0) {
        throw std::runtime_error(std::string("getaddrinfo error: ") + gai_strerror(status));
    }

    for (p = res; p!= nullptr; p = p->ai_next) {
        sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sock < 0) {
            continue;
        }

        if (connect(sock, p->ai_addr, p->ai_addrlen) == 0) {
            break;
        }

        close(sock);
    }

    if (p == nullptr) {
        freeaddrinfo(res);
        throw std::runtime_error("Failed to connect to server");
    }

    freeaddrinfo(res);
    std::cout << "Connected to server at " << hostname << ":" << port << "\n";
}


/**
 * @brief Entry point of the FTP client program.
 * 
 * Parses command-line arguments for server IP and port, connects to the server, 
 * and starts the interactive client loop.
 * 
 * @param argc Number of command-line arguments.
 * @param argv Array of command-line arguments. Expects <server_ip> and <port>.
 * 
 * @return 0 on successful execution, 1 on failure.
 */
int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << "<server_ip> <port> \n";
        return 1;
    }

    std::string hostname = argv[1];
    int port = std::stoi(argv[2]);

    int sock;
    try {
        connect_to_server(hostname, port, sock);
        client_loop(sock);
        close(sock);
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}