#include <myftp.h>
#include <iostream>
#include <unistd.h>
#include <stdexcept>
#include <fstream>
#include <arpa/inet.h>
#include <cstring>
#include <sys/socket.h>
#include <netdb.h>
#include <thread>


#define BUFFER_SIZE 1024


std::string receive_response(int sock) {
    char message_buffer[BUFFER_SIZE];
    ssize_t bytes_received = recv(sock, message_buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received <= 0) {
        throw std::runtime_error("Disconnected from server.");
    }

    message_buffer[bytes_received] = '\0';
    return std::string(message_buffer);
}


void send_command(int sock, const std::string &command) {
    send(sock, command.c_str(), command.size(), 0);
}


void send_terminate_request(int terminate_sock, int command_id) {
    std::string terminate_command = "terminate " + std::to_string(command_id) + "\n";
    send(terminate_sock, terminate_command.c_str(), terminate_command.size(), 0);
    std::cout << "Sent termination request for Command-ID: " << command_id << std::endl;
}


void handle_get(int sock, const std::string &filename) {
    send_command(sock, "get " + filename);
    std::string response = receive_response(sock);
    std::cout << response;
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


void client_loop(int sock, int terminate_sock) {
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
            std::thread put_thread(handle_put, sock, filename);
            put_thread.detach();

        } else if (command.substr(0, 4) == "get ") {
            std::string filename = command.substr(4);
            std::thread get_thread(handle_get, sock, filename);
            get_thread.detach();

        } else if (command.substr(0, 9) == "terminate") {
            int command_id = std::stoi(command.substr(10));
            send_terminate_request(terminate_sock, command_id); 

        } else {
            send_command(sock, command);
            std::string response = receive_response(sock);
            std::cout << response;
        }
    }
}


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


int main(int argc, char *argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << "<server_ip> <nport> <tport> \n";
        return 1;
    }

    std::string hostname = argv[1];
    int nport = std::stoi(argv[2]);
    int tport = std::stoi(argv[3]);

    int sock, terminate_sock;
    try {
        connect_to_server(hostname, nport, sock);
        connect_to_server(hostname, tport, terminate_sock);

        client_loop(sock, terminate_sock);

        close(sock);
        close(terminate_sock);
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}