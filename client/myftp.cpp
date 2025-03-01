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
std::string hostname;
int tport;


bool remove_file(const std::string &path) {
    return (remove(path.c_str()) == 0);
}

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
    std::cout << "Connected to server at " << hostname << " PORT: \e[0;34m" << port << "\e[0;0m\n";
}


std::pair<int, int> parse_command_id_data_port(std::string &response) {
    int data_port = -1;
    std::size_t data_port_pos = response.find("DATA_PORT");
    if (data_port_pos != std::string::npos) {
        data_port = std::stoi(response.substr(data_port_pos + 10)); // 10 to skip "DATA_PORT "
    }

    int command_id = -1;
    std::size_t command_id_pos = response.find("Command-ID");
    if (command_id_pos != std::string::npos) {
        command_id = std::stoi(response.substr(command_id_pos + 11)); // 11 to skip "Command-ID "
    }

    return { command_id, data_port };
}


void handle_get(int sock, const std::string &filename) {
    send_command(sock, "get " + filename);
    std::string response = receive_response(sock);

    if (response.find("SUCCESS") == std::string::npos) {  // If there is an Error
        std::cerr << response << "Server is not ready to send" << "\n";
        return;
    }

    std::pair<int, int> result = parse_command_id_data_port(response);
    int command_id = result.first;
    int data_port = result.second;
    std::cout << "Command-ID: " << command_id << ", Data Port: " << data_port << std::endl;

    int data_sock;
    connect_to_server(hostname, data_port, data_sock);

    std::thread get_data_thread ([data_sock, filename] () {
        std::ofstream file(filename, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Error: Unable to create local file.\n";
            return;
        }

        char buffer[BUFFER_SIZE];
        while (true) {
            ssize_t bytes_received = recv(data_sock, buffer, BUFFER_SIZE, 0);
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
    });
    get_data_thread.detach();
}


void handle_put(int sock, const std::string &filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Client Error: Unable to open file.\n";
        return;
    }

    send_command(sock, "put " + filename);
    std::string response = receive_response(sock); 

    if (response.find("SUCCESS") == std::string::npos) {  // If there is an Error
        std::cerr << response << "Server not ready to Receive" << "\n";
        return;
    }

    std::pair<int, int> result = parse_command_id_data_port(response);
    int command_id = result.first;
    int data_port = result.second;
    std::cout << "Command-ID: " << command_id << ", Data Port: " << data_port << std::endl;

    int data_sock;
    connect_to_server(hostname, data_port, data_sock);

    std::thread put_data_thread(
        [data_sock, filename]() {
            std::ifstream file(filename, std::ios::binary);

            std:: cout << "Transferring File\n";

            char buffer[BUFFER_SIZE];
            while (file.read(buffer, sizeof(buffer))) {
                send(data_sock, buffer, file.gcount(), 0);
            }

            if (file.gcount() > 0) {
                send(data_sock, buffer, file.gcount(), 0);
            }
            
            std::string end_message = "FILE_TRANSFER_END\n";
            send(data_sock, end_message.c_str(), end_message.size(), 0);
            std::cout << "You sent a file: " << filename << "\n";
            
            std::string response = receive_response(data_sock);
            std::cout << response << "\n"; 

            close(data_sock);
    });
    put_data_thread.detach();
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


int main(int argc, char *argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << "<server_ip> <nport> <tport> \n";
        return 1;
    }

    hostname = argv[1];
    int nport = std::stoi(argv[2]);
    tport = std::stoi(argv[3]);

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