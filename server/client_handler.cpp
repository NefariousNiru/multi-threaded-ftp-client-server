#include "client_handler.h"
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <dirent.h>
#include <sys/socket.h>
#include <filesystem>
#include <sys/stat.h> 
#include <fstream>
#include <unordered_map>
#include <functional>
#include <cerrno>
#include <cstring>


#define BUFFER_SIZE 1024


/**
 * @brief Sends a standardized response to the client.
 * 
 * This function formats the response as "<status>: <message>\n" and sends it to the client.
 * It is useful for maintaining consistency in server-client communication.
 * 
 * @param sock The client's socket file descriptor.
 * @param status A short status string (e.g., "SUCCESS", "ERROR") indicating the result of the operation.
 * @param message A detailed message providing context or additional information about the status.
 */ 
void send_response(int sock, const std::string &status, const std::string &message) {
    std::string response = status + ": " + message + "\n";
    send(sock, response.c_str(), response.size(), 0);
}


/**
 * @brief Sends a standardized response to the client.
 * 
 * This function formats the response as "<message>\n" and sends it to the client.
 * It is useful for maintaining consistency in server-client communication.
 * 
 * @param sock The client's socket file descriptor.
 * @param message A detailed message providing context or additional information about the status.
 */ 
void send_response(int sock, const std::string &message) {
    std::string response = message + "\n";
    send(sock, response.c_str(), response.size(), 0);
}


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
 * @brief Receives a file from the client and saves it on the server.
 * 
 * @param sock The client's socket file descriptor.
 * @param filename The name of the file to save on the server.
 */
void handle_put(int sock, const std::string &filename) {
    if (filename.empty()) {
        send_response(sock, "ERROR", "File name not specified.");
        return;
    }

    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        send_response(sock, "ERROR", "Unable to create file.");
        return;
    }

    send_response(sock, "SUCCESS", "READY_TO_RECEIVE");

    char buffer[BUFFER_SIZE];
    std::string leftover_data;
    while (true) {
        ssize_t bytes_received = recv(sock, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            break;
        }

        leftover_data.append(buffer, bytes_received);

        // Check the termination marker 
        size_t end_position = leftover_data.find("FILE_TRANSFER_END\n");
        if (end_position != std::string::npos) {
            file.write(leftover_data.c_str(), end_position);
            break;
        }

        file.write(leftover_data.c_str(), leftover_data.size());
        leftover_data.clear();
    }

    file.close();

    if (leftover_data.empty()) {
        send_response(sock, "ERROR", "File transfer failed.");
    } else {
        send_response(sock, "SUCCESS", "File transfer completed.");
    }
}


/**
 * @brief Sends a file from the server to the client.
 * 
 * @param sock The client's socket file descriptor.
 * @param filename The name of the file to send.
 */
void handle_get(int sock, const std::string &filename) {
    if (filename.empty()) {
        send_response(sock, "ERROR", "File name not specified.");
        return;
    }

    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        switch (errno) {
            case ENOENT:
                send_response(sock, "ERROR", "404 - File not found.");
                break;
            case EACCES:
                send_response(sock, "ERROR", "Permission denied.");
                break;
            case EISDIR:
                send_response(sock, "ERROR", "Path is a directory, not a file.");
                break;
            default:
                send_response(sock, "ERROR", std::string("Unable to open file: ") + std::strerror(errno));
                break;
        }
        return;
    }

    send_response(sock, "SUCCESS", "FILE_TRANSFER_START");

    char buffer[BUFFER_SIZE];
    while (file.read(buffer, sizeof(buffer))) {
        // Binary files - Do not use send_response()
        send(sock, buffer, file.gcount(), 0);
    }

    if (file.gcount() > 0) {
        send(sock, buffer, file.gcount(), 0);
    }

    send_response(sock, "FILE_TRANSFER_END");

    file.close();
}


/**
 * @brief Creates a new directory in the current working directory.
 * 
 * @param sock The client's socket file descriptor.
 * @param directory_name The name of the new directory to create.
 */
void handle_mkdir(int sock, const std::string &directory_name) {
    if (directory_name.empty()) {
        send_response(sock, "ERROR", "Directory name not specified.");
        return;
    }

    if (mkdir(directory_name.c_str(), 0755) == 0) {
        send_response(sock, "SUCCESS", "Directory created successfully.");
    } else {
        send_response(sock, "ERROR", "Unable to create directory.");
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
        send_response(sock, "ERROR", "File name not specified.");
        return;
    }

    if (remove(filename.c_str()) == 0) {
        send_response(sock, "SUCCESS", "File deleted successfully.");
    } else {
        switch (errno) {
            case ENOENT:
                send_response(sock, "ERROR", "404 - File not found.");
                break;
            case EACCES:
                send_response(sock, "ERROR", "Permission denied.");
                break;
            case EPERM:
                send_response(sock, "ERROR", "Operation not permitted.");
                break;
            case EISDIR:
                send_response(sock, "ERROR", "Cannot delete a directory using this command.");
                break;
            default:
                send_response(sock, "ERROR", std::strerror(errno));
                break;
        }
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
        send_response(sock, "ERROR", "Directory not specified.");
        return;
    }

    if (chdir(directory.c_str()) == 0) {
        send_response(sock, "SUCCESS", "Directory changed.");
    } else {
        send_response(sock, "ERROR", "Unable to change directory.");
    }
}


/**
 * @brief Lists files and directories in the current directory.
 * 
 * @param sock The client's socket file descriptor.
 */
void handle_ls(int sock) {
    DIR *dir = opendir(".");
    if (dir == nullptr) {
        send_response(sock, "ERROR", "Unable to open directory.");
        return;
    }

    struct dirent *entry;
    std::string file_list;
    while ((entry = readdir(dir)) != nullptr) {
        file_list += entry -> d_name;
        file_list += "\n";
    }

    closedir(dir);
    if (!file_list.empty()) {
        send_response(sock, file_list);
    } else {
        send_response(sock, "SUCCESS", "Directory is empty.");
    }
}


/**
 * @brief Prints the current working directory.
 * 
 * @param sock The client's socket file descriptor.
 */
void handle_pwd(int sock) {
    char cwd[BUFFER_SIZE];
    if (getcwd(cwd, sizeof(cwd)) != nullptr) {
        send_response(sock, std::string(cwd));
    } else {
        send_response(sock, "ERROR", "Unable to retrieve current directory.");
    }
}


using CommandMap = std::unordered_map<std::string, std::function<void(int, const std::string &)>>;
/**
 * @brief Creates and initializes the command map.
 * 
 * This function sets up the `CommandMap` with supported FTP commands and their 
 * corresponding handler functions. Commands are categorized into those with 
 * and without arguments:
 * 
 * - Commands without arguments:
 *   - "pwd" -> Calls `handle_pwd` to print the current working directory.
 *   - "ls" -> Calls `handle_ls` to list files and directories in the current directory.
 * 
 * - Commands with arguments:
 *   - "cd <directory>" -> Calls `handle_cd` to change the current working directory.
 *   - "mkdir <directory>" -> Calls `handle_mkdir` to create a new directory.
 *   - "delete <filename>" -> Calls `handle_delete` to delete a file.
 *   - "get <filename>" -> Calls `handle_get` to send a file to the client.
 *   - "put <filename>" -> Calls `handle_put` to receive a file from the client.
 * 
 * @return CommandMap The initialized map associating command strings with their handlers.
 */
CommandMap create_command_map() {
    CommandMap command_map;

    // Commands without arguments
    command_map["pwd"] = [](int sock, const std::string &) { handle_pwd(sock); };
    command_map["ls"] = [](int sock, const std::string &) { handle_ls(sock); };

    // Commands with arguments
    command_map["cd"] = [](int sock, const std::string &arg) { handle_cd(sock, arg); };
    command_map["mkdir"] = [](int sock, const std::string &arg) { handle_mkdir(sock, arg); };
    command_map["delete"] = [](int sock, const std::string &arg) { handle_delete(sock, arg); };
    command_map["get"] = [](int sock, const std::string &arg) { handle_get(sock, arg); };
    command_map["put"] = [](int sock, const std::string &arg) { handle_put(sock, arg); };

    return command_map;
}


/**
 * @brief Parses and executes a command received from the client.
 * 
 * @param command The command string received from the client.
 * @param sock The client's socket file descriptor.
 */
void execute_command(const std::string &command, int sock) {
    // Create the command map
    static CommandMap command_map = create_command_map();

    // Parse the command and argument
    size_t space_pos = command.find(' ');
    std::string cmd = (space_pos == std::string::npos) ? command : command.substr(0, space_pos);
    std::string arg = (space_pos == std::string::npos) ? "" : trim(command.substr(space_pos + 1));

    // Find the command in the map
    auto it = command_map.find(cmd);
    if (it != command_map.end()) {
        it->second(sock, arg);
    } else {
        send_response(sock, "ERROR", "Invalid command.");
    }
}


/**
 * @brief Handles a single client connection.
 * 
 * @param sock The client's socket file descriptor.
 */
void handle_client(int sock) {
    const char *welcome_msg = "\033[32mConnected to MyFTPServer!\033[0m";
    send_response(sock, welcome_msg);

    char buffer[BUFFER_SIZE];
    while (true) {
        memset(buffer, 0, BUFFER_SIZE);

        // Receive commands from client 
        ssize_t bytes_received = recv(sock, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received <= 0) {
            std::cout << "\033[31mClient Disconnected.\033[0m\n";
            break;
        }

        std::string command(buffer);
        command = trim(command); 
        if (command.empty()) {
            send_response(sock, "");
        }

        if (command == "quit") {
            std::cout << "\033[31mClient Disconnected.\033[0m\n";
            break;
        } 

        execute_command(command, sock);
    }

    close(sock);
}