#include <iostream>
#include <cstring>
#include <unistd.h>
#include <dirent.h>
#include <sys/socket.h>
#include <sys/stat.h> 
#include <sys/types.h>
#include <fstream>
#include <unordered_map>
#include <functional>
#include <cerrno>
#include <cstring>
#include <cstdio>
#include <dirent.h>
#include <vector>
#include <string>
#include <atomic>
#include <mutex>
#include <netinet/in.h>
#include <thread>
#include <arpa/inet.h>
#include "util.h"


bool create_data_socket(int &data_sock_out, int &ephemeral_port_out) {
    data_sock_out = socket(AF_INET6, SOCK_STREAM, 0);
    if (data_sock_out == -1) {
        std::cerr << "Could not create data socket.\n";
        return false;
    }

    // Bind to an ephemeral port (port 0 => ephemeral)
    sockaddr_in6 addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin6_family = AF_INET6;
    addr.sin6_addr   = in6addr_any;
    addr.sin6_port   = 0; // 0 => ephemeral port (OS decides)

    if (bind(data_sock_out, (sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "Failed to bind ephemeral data socket.\n";
        close(data_sock_out);
        return false;
    }

    socklen_t addr_len = sizeof(addr);
    if (getsockname(data_sock_out, (sockaddr*)&addr, &addr_len) < 0) {
        std::cerr << "Failed to get ephemeral port.\n";
        close(data_sock_out);
        return false;
    }

    ephemeral_port_out = ntohs(addr.sin6_port);

    if (listen(data_sock_out, 1) < 0) {
        std::cerr << "Listen on ephemeral data socket failed.\n";
        close(data_sock_out);
        return false;
    }

    return true;
}


bool file_exists(const std::string &path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}


bool create_directory(const std::string &path) {
    return (mkdir(path.c_str(), 0755) == 0);
}


bool remove_file(const std::string &path) {
    return (remove(path.c_str()) == 0);
}


void send_response_impl(int sock, const std::string &response) {
    ssize_t bytes_sent = send(sock, response.c_str(), response.size(), 0);
    if (bytes_sent == -1) {
        std::cerr << "Error sending response: " << strerror(errno) << std::endl;
    }
}


void send_response(int sock, const std::string &status, const std::string &message) {
    std::string response = status + ": " + message + "\n";
    send_response_impl(sock, response);
}


void send_response(int sock, const std::string &message) {
    std::string response = message + "\n";
    send_response_impl(sock, response);
}


std::string trim(const std::string &str) {
    size_t start = str.find_first_not_of(" \t\n\r");
    size_t end = str.find_last_not_of(" \t\n\r");
    return (start == std::string::npos) ? "" : str.substr(start, end - start + 1);
}

