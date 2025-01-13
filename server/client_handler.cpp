#include "client_handler.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <dirent.h>
#include <sys/socket.h>
#include <filesystem>
#include <sys/stat.h> 

#define BUFFER_SIZE 1024


/**
 * @brief Trims trailing whitespace, including '\n' and '\r'
 * 
 * @param str  String to be cleaned
 */
std::string trim(const std::string &str) {
    size_t start = str.find_first_not_of(" \t\n\r");
    size_t end = str.find_last_not_of(" \t\n\r");
    return (start == std::string::npos) ? "" : str.substr(start, end - start + 1);
}


/**
 * @brief Creates a new directory in the current working directory.
 * 
 * @param sock The client's socket file descriptor.
 * @param directory_name The name of the new directory to create.
 */
void handle_mkdir(int sock, const std::string &directory_name) {
    if (directory_name.empty()) {
        const char *error_msg = "Error: Directory name not specified.\n";
        send(sock, error_msg, strlen(error_msg), 0);
        return;
    }

    if (mkdir(directory_name.c_str(), 0755) == 0) {
        const char *success_msg = "Directory created successfully.\n";
        send(sock, success_msg, strlen(success_msg), 0);
    } else {
        const char *error_msg = "Error: Unable to create directory.\n";
        send(sock, error_msg, strlen(error_msg), 0);
    }
}


/**
 * @brief Deletes a file from the server's current working directory.
 * 
 * @param sock The client's socket file descriptor.
 * @param filename The name of the file to delete.
 */
void handle_delete(int sock, const std::string &filename) {
    if (filename.empty()) {
        const char *error_msg = "Error: File name not specified.\n";
        send(sock, error_msg, strlen(error_msg), 0);
        return;
    }

    if (remove(filename.c_str()) == 0) {
        const char *success_msg = "File deleted successfully.\n";
        send(sock, success_msg, strlen(success_msg), 0);
    } else {
        const char *error_msg = "Error: Unable to delete file.\n";
        send(sock, error_msg, strlen(error_msg), 0);
    }
}


/**
 * @brief Changes the current working directory on the server.
 * 
 * @param sock The client's socket file descriptor.
 * @param directory The target directory to change to.
 */
void handle_cd(int sock, const std::string &directory) {
    if (directory.empty()) {
        const char *error_msg = "Error: Directory not specified.\n";
        send(sock, error_msg, strlen(error_msg), 0);
        return;
    }

    if (chdir(directory.c_str()) == 0) {
        const char *success_msg = "Directory changed successfully.\n";
        send(sock, success_msg, strlen(success_msg), 0);
    } else {
        const char *error_msg = "Error: Unable to change directory.\n";
        send(sock, error_msg, strlen(error_msg), 0);
    }
}


/**
 * @brief Lists files and directories in the current directory.
 * 
 * @param sock The client's socket file descriptor.
 */
void handle_ls(int sock) {
    std::cout << "im here" ;
    DIR *dir = opendir(".");
    if (dir == nullptr) {
        const char *error_msg = "Error: Unable to open directory.\n";
        send(sock, error_msg, strlen(error_msg), 0);
        return;
    }

    struct dirent *entry;
    std::string file_list;
    while ((entry = readdir(dir)) != nullptr) {
        file_list += entry -> d_name;
        file_list += "\n";
    }

    closedir(dir);
    send(sock, file_list.c_str(), file_list.size(), 0);
    send(sock, "\n", 1, 0);
}


/**
 * @brief Prints the current working directory.
 * 
 * @param sock The client's socket file descriptor.
 */
void handle_pwd(int sock) {
    char cwd[BUFFER_SIZE];
    if (getcwd(cwd, sizeof(cwd)) != nullptr) {
        send(sock, cwd, strlen(cwd), 0);
        send(sock, "\n", 1, 0);
    } else {
        const char *error_msg = "Error: Unable to retrieve current directory.\n";
        send(sock, error_msg, strlen(error_msg), 0);
    }
}


/**
 * @brief Parses and executes a command received from the client.
 * 
 * @param command The command string received from the client.
 * @param sock The client's socket file descriptor.
 */
void execute_command(const std::string &command, int sock) {
    if (command.compare("pwd") == 0) {
        handle_pwd(sock);
    } else if (command.compare("ls") == 0) {
        handle_ls(sock);
    } else if (command.starts_with("cd ")) {
        std::string directory = command.substr(3);
        handle_cd(sock, trim(directory));
    } else if (command.starts_with("mkdir ")) {
        std::string directory_name = command.substr(6);
        handle_mkdir(sock, trim(directory_name));
    } else if (command.starts_with("delete ")) {
        std::string filename = command.substr(7);
        handle_delete(sock, trim(filename));
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
    const char *welcome_msg = "Connected to MyFTPServer!";
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
        command = trim(command); 

        if (command == "quit") {
            break;
        } 

        execute_command(command, sock);
    }

    close(sock);
}