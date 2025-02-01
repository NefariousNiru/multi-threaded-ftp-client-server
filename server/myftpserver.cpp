#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <thread>
#include <arpa/inet.h>
#include "thread_pool.h"
#include "client_handler.h"
#include <unordered_map>


#define THREAD_POOL_SIZE (std::thread::hardware_concurrency() > 0 ? std::thread::hardware_concurrency() : 4)
#define BACKLOG_QUEUE_SIZE 64
#define BUFFER_SIZE 1024


std::unordered_map<int, bool> active_commands; // Command ID -> Running status
std::mutex command_mutex;


/**
 * @brief Handles incoming terminate requests.
 * 
 * @param tport The terminate port number.
 */
void handle_terminate_requests(int tport) {
    int term_sock = socket(AF_INET6, SOCK_STREAM, 0);
    if (term_sock == -1) {
        std::cerr << "Failed to create terminate socket." << std::endl;
        return;
    }

    sockaddr_in6 term_addr{};
    term_addr.sin6_family = AF_INET6;
    term_addr.sin6_addr = in6addr_any;
    term_addr.sin6_port = htons(tport);

    if (bind(term_sock, (struct sockaddr *)&term_addr, sizeof(term_addr)) < 0) {
        std::cerr << "Binding terminate socket failed." << std::endl;
        close(term_sock);
        return;
    }

    if (listen(term_sock, BACKLOG_QUEUE_SIZE) < 0) {
        std::cerr << "Listen on terminate socket failed." << std::endl;
        close(term_sock);
        return;
    }

    std::cout << "Listening for termination requests @ PORT: \e[0;34m" << tport << "\e[0;0m\n";

    while (true) {
        sockaddr_in6 client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(term_sock, (sockaddr *)&client_addr, &client_len);

        if (client_sock < 0) {
            std::cerr << "Failed to accept terminate request." << std::endl;
            continue;
        }

        char buffer[BUFFER_SIZE];
        ssize_t bytes_received = recv(client_sock, buffer, BUFFER_SIZE, 0);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            int command_id = std::stoi(buffer + 10); // Extract command ID
            std::lock_guard<std::mutex> lock(command_mutex);
            if (active_commands.count(command_id)) {
                active_commands[command_id] = false; // Mark command for termination
                std::cout << "Terminating command ID " << command_id << std::endl;
                active_commands.erase(command_id);
            }
        }
        close(client_sock);
    }
}



/**
 * @brief Creates a dual-stack IPv4 and IPv6 socket.
 * 
 * @return int The socket file descriptor on success, or -1 on failure.
 */
int create_socket() {
    int sock = socket(AF_INET6, SOCK_STREAM, 0);
    if (sock == - 1) {
        std::cerr << "Failed to create socket. Exiting! \n";
        return -1;
    }
    std::cout << "Socket created successfully. \n";
    return sock;
}


/**
 * @brief Configures the socket to support both IPv4 and IPv6 connections.
 * 
 * @param sock The socket file descriptor.
 * @return bool True if the configuration is successful, otherwise false.
 */
bool set_dual_stack(int sock) {
    // Accept both IPv4 & IPv6
    int opt = 0;
    if(setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt)) <  0) {
        std::cerr << "Failed to set dual-stack mode. \n";
        return false;
    }
    std::cout << "Socket set to dual-stack mode - (IPv4 & IPv6). \n";
    return true;
}


/**
 * @brief Binds the socket to a specified port for accepting connections.
 * 
 * @param sock The socket file descriptor.
 * @param server_addr The sockaddr_in6 structure representing the server address.
 * @return bool True if binding is successful, otherwise false.
 */
bool bind_socket(int sock, sockaddr_in6 &server_addr, int PORT) {
    // Define server address structure to the specified port 
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin6_family = AF_INET6;
    server_addr.sin6_addr   = in6addr_any;
    server_addr.sin6_port   = htons(PORT);

    // Bind socket to PORT
    if (bind(sock, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Binding to PORT failed. \n";
        close(sock);
        return false;
    }
    std::cout << "Socket binded successfully. \n";
    return true;
}


/**
 * @brief Puts the socket into listening mode for incoming connections.
 * 
 * @param sock The socket file descriptor.
 * @return bool True if the socket successfully starts listening, otherwise false.
 */
bool start_listening(int sock, int PORT) {
    // Listen for Incoming TCP-Connection
    if (listen(sock, BACKLOG_QUEUE_SIZE) < 0) {
        std::cerr << "Listen failed. \n";
        close(sock);
        return false;
    }
    std::cout << "Server is listening @ PORT: \e[0;34m" << PORT << "\e[0;0m\n";
    return true;
}


/**
 * @brief Retrieves the IP address of the connected client.
 * 
 * @param client_addr The sockaddr_in6 structure representing the client's address.
 * @return std::string The client's IP address as a human-readable string.
 */
std::string get_client_ip(const sockaddr_in6 &client_addr) {
    char client_ip[INET6_ADDRSTRLEN];

    // Check if the address is IPv4-mapped IPv6
    if (IN6_IS_ADDR_V4MAPPED(&client_addr.sin6_addr)) {
        // Extract IPv4-mapped address and convert it to pure IPv4
        sockaddr_in ipv4_addr;
        memset(&ipv4_addr, 0, sizeof(ipv4_addr));
        ipv4_addr.sin_family = AF_INET;
        ipv4_addr.sin_addr.s_addr = *reinterpret_cast<const uint32_t*>(&client_addr.sin6_addr.s6_addr[12]);
        inet_ntop(AF_INET, &ipv4_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    } else {
        // Standard IPv6 address
        inet_ntop(AF_INET6, &client_addr.sin6_addr, client_ip, INET6_ADDRSTRLEN);
    }
    return std::string(client_ip);
}


/**
 * @brief Accepts and processes incoming client connections.
 * 
 * @param server_sock The server's socket file descriptor.
 */
void accept_incoming_connections(int server_sock, int tport) {
    ThreadPool pool(THREAD_POOL_SIZE);
    std::thread terminate_thread(handle_terminate_requests, tport);
    terminate_thread.detach();

    while (true) {
        sockaddr_in6 client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(server_sock, (sockaddr*)&client_addr, &client_len);

        if (client_sock < 0) {
            std::cerr << "Failed to accept Client Connection";
            continue;
        }

        std::string client_ip = get_client_ip(client_addr);
        std::cout << "\033[32mClient connected from IP: " << client_ip << "\033[0m\n";

        pool.enqueue([client_sock]() {
            handle_client(client_sock);
        });
    }
}


/**
 * @brief The main entry point of the server application.
 * 
 * @return int Exit code (0 for success, 1 for failure).
 */
int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <nport> <tport>" << std::endl;
        return 1;
    }

    int nport = std::stoi(argv[1]);
    int tport = std::stoi(argv[2]);

    int server_sock = create_socket();
    if (server_sock == -1) return 1;

    if (!set_dual_stack(server_sock)) {
        close(server_sock);
        return 1;
    }

    sockaddr_in6 server_addr;
    if (!bind_socket(server_sock, server_addr, nport)) {
        close(server_sock);
        return 1;
    }

    if (!start_listening(server_sock, nport)) {
        close(server_sock);
        return 1;
    }

    accept_incoming_connections(server_sock, tport);

    close(server_sock);
    std::cout << "Server shut down.\n";
    return 0;
}