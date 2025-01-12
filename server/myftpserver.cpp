#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <thread>
#include <arpa/inet.h>
#include "thread_pool.h"


#define THREAD_POOL_SIZE (std::thread::hardware_concurrency() > 0 ? std::thread::hardware_concurrency() : 4)
#define PORT 8080
#define BACKLOG_QUEUE_SIZE 64
#define BUFFER_SIZE 1024


int create_socket() {
    // Create a dual-stack IPv4 and IPv6 TCP-SOCKET
    int sock = socket(AF_INET6, SOCK_STREAM, 0);
    if (sock == - 1) {
        std::cerr << "Failed to create socket. Exiting! \n";
        return 1;
    }
    std::cout << "Socket created successfully. \n";
    return sock;
}


bool set_dual_stack(int sock) {
    // Accept both IPv4 & IPv6
    int opt = 0;
    if(setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt)) <  0) {
        std::cerr << "Failed to set dual-stack mode. \n";
        close(sock);
        return false;
    }
    std::cout << "Socket set to dual-stack mode - (IPv4 & IPv6). \n";
    return true;
}


bool bind_socket(int sock, sockaddr_in6 &server_addr) {
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


bool start_listening(int sock) {
    // Listen for Incoming TCP-Connection
    if (listen(sock, BACKLOG_QUEUE_SIZE) < 0) {
        std::cerr << "Listen failed. \n";
        close(sock);
        return false;
    }
    std::cout << "Server is listening @ PORT: \e[0;34m" << PORT << "\e[0;0m\n";
    return true;
}


void handle_client(int sock) {
    const char *welcome_msg = "Welcome to the dual-stack server!\n";
    send(sock, welcome_msg, strlen(welcome_msg), 0);
    std::cout << "Sent welcome message to the client.\n";
    close(sock);
    std::cout << "Client connection closed.\n";
}


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


void accept_incoming_connections(int server_sock) {
    // Accept multiple client connections in a loop

    ThreadPool pool(THREAD_POOL_SIZE);

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

        pool.enqueue([client_sock, client_addr]() {
            handle_client(client_sock);
        });
    }
}

int main() {
    // Create a dual-stack socket - Accept both IPv6 and IPv4
    int server_sock = create_socket();
    if (server_sock == -1) return 1;

    //  Set dual-stack mode
    if (!set_dual_stack(server_sock)) {
        close(server_sock);
        return 1;
    }
    
    // Bind the socket
    sockaddr_in6 server_addr;
    if (!bind_socket(server_sock, server_addr)) {
        close(server_sock);
        return 1;
    }

    //  Lsiten for connections
    if (!start_listening(server_sock)) {
        close(server_sock);
        return 1;
    }

    // Accept incoming connections for the server_socket
    accept_incoming_connections(server_sock);

    // Clean up and close sockets
    close(server_sock);
    std::cout << "Server shut down.\n";
    return 0;
}