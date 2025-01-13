#include "client_handler.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <dirent.h>
#include <sys/socket.h>


#define BUFFER_SIZE 1024


/**
 * @brief Parses and executes a command received from the client.
 * 
 * @param command The command string received from the client.
 * @param sock The client's socket file descriptor.
 */
void execute_command(const std::string &command, int sock) {
    if (command == "pwd") {
        handle_pwd(sock);
    } else if (command == "ls") {
        handle_ls(sock);
    } else {
        const char *error_msg = "Invalid command. \n";
        send(sock, error_msg, strlen(error_msg), 0);
    }
}


/**
 * @brief Handles a single client connection.
 * 
 * @param sock The client's socket file descriptor.
 */
void handle_client(int sock) {
    const char *welcome_msg = "Welcome to the FTP-Server! Command Away";
    send(sock, welcome_msg, strlen(welcome_msg), 0);

    char buffer[BUFFER_SIZE];
    while (true) {
        memset(buffer, 0, BUFFER_SIZE);

        // Receive commands from client 
        ssize_t bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received <= 0) {
            std::cout << "Client Disconnected. \n";
            break;
        }

        std::string command(buffer);
        std::cout << "Received command: " << command << "\n";

        if (command == "quit") {
            break;
        } 

        execute_command(command, sock);
    }

    close(sock);
}