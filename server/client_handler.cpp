#include "client_handler.h"
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


#define BUFFER_SIZE 1024
std::atomic<int> next_command_id{1};  // Global command ID counter
extern std::unordered_map<int, bool> active_commands;
extern std::mutex command_mutex;


bool create_data_socket(int &data_sock_out, int &ephemeral_port_out) {
    data_sock_out = socket(AF_INET6, SOCK_STREAM, 0);
    if (data_sock_out == -1) {
        std::cerr << "Could not create data socket.\n";
        return false;
    }

    // Bind to an ephemeral port (port 0 => ephemeral)
    sockaddr_in6 addr{};
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


void receive_data_put(int sock, int command_ID, const std::string &filename) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        send_response(sock, "ERROR", "Unable to create file.");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(command_mutex);
        active_commands[command_ID] = true;
    }

    char buffer[BUFFER_SIZE];
    std::string leftover_data;
    while (true) {
        ssize_t bytes_received = recv(sock, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            break;
        }
        
        {
            std::lock_guard<std::mutex> lock(command_mutex);
            if (!active_commands[command_ID]) {
                std::cerr << "Transfer aborted for Command-ID: " << command_ID << std::endl;
                file.close();
                remove_file(filename); // Delete the incomplete file
                send_response(sock, "ERROR", "Transfer aborted.");
                return;
            }
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

    {
        std::lock_guard<std::mutex> lock(command_mutex);
        active_commands.erase(command_ID);
    }

    if (leftover_data.empty()) {
        send_response(sock, "ERROR", "File transfer failed.");
    } else {
        send_response(sock, "SUCCESS", "File transfer completed.");
    }

    close(sock);
}


void handle_put(int sock, const std::string &filename) {
    if (filename.empty()) {
        send_response(sock, "ERROR", "File name not specified.");
        return;
    }

    if (file_exists(filename)) {
        send_response(sock, "ERROR", "File with the same name exists.");
        return;
    }

    // Create data socket and ephemeral port
    int data_sock, ephemeral_port;
    if (!create_data_socket(data_sock, ephemeral_port)) {
        send_response(sock, "ERROR", "Failed to setup data channel.");
        return;
    }

    // This is the termination command id
    int command_ID = next_command_id++;

    // Send ephermeral port number + command ID to client
    std::string msg = "DATA_PORT " + std::to_string(ephemeral_port) + " Command-ID: " + std::to_string(command_ID);
    send_response(sock, "SUCCESS", msg);


    // Use the socket to send data over a new thread
    std::thread t([=](){
        sockaddr_in6 data_client_addr;
        socklen_t len = sizeof(data_client_addr);

        int data_client_sock = accept(data_sock, (sockaddr*)&data_client_addr, &len);
        close(data_sock); // Once we accept connection we dont need to lsiten anymore so close it

        if (data_client_sock < 0) {
            std::cerr << "Failed to accept data connection.\n";
            return;
        }
        receive_data_put(data_client_sock, command_ID, filename);
    });

    t.detach();
}


void handle_get(int sock, const std::string &filename) {
    if (filename.empty()) {
        send_response(sock, "ERROR", "File name not specified.");
        return;
    }

    if (!file_exists(filename)) {
        send_response(sock, "ERROR", "404 - File not found.");
        return;
    }

    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        send_response(sock, "ERROR", "Unable to open file.");
        return;
    }

    int command_ID = next_command_id++;
    {
        std::lock_guard<std::mutex> lock(command_mutex);
        active_commands[command_ID] = true;
    }

    send_response(sock, "SUCCESS", "FILE_TRANSFER_START Command-ID: " + std::to_string(command_ID));

    char buffer[BUFFER_SIZE];
    while (file.read(buffer, sizeof(buffer))) {
        {
            std::lock_guard<std::mutex> lock(command_mutex);
            if (!active_commands[command_ID]) {
                std::cerr << "Transfer aborted for Command-ID: " << command_ID << std::endl;
                file.close();
                send_response(sock, "ERROR", "Transfer aborted.");
                return;
            }
        }

        // Binary files - Do not use send_response()
        ssize_t sent = send(sock, buffer, file.gcount(), 0);
        if (sent <= 0) {
            std::cerr << "Error: Failed to send data to client.\n";
            file.close();
            return;
        }
    }

    if (file.gcount() > 0) {
        ssize_t sent = send(sock, buffer, file.gcount(), 0);
        if (sent <= 0) {
            std::cerr << "Error: Failed to send final data to client.\n";
            file.close();
            return;
        }
    }

    send_response(sock, "FILE_TRANSFER_END");

    file.close();

    {
        std::lock_guard<std::mutex> lock(command_mutex);
        active_commands.erase(command_ID);
    }
}


void handle_mkdir(int sock, const std::string &directory_name) {
    if (directory_name.empty()) {
        send_response(sock, "ERROR", "Directory name not specified.");
        return;
    }

    struct stat path_stat;
    if (stat(directory_name.c_str(), &path_stat) == 0) {
        if (S_ISDIR(path_stat.st_mode)) {
            send_response(sock, "ERROR", "Directory already exists.");
        } else {
            send_response(sock, "ERROR", "A file with the same name exists.");
        }
        return;
    }

    if (create_directory(directory_name)) {
        send_response(sock, "SUCCESS", "Directory created successfully.");
    } else {
        std::cerr << "Error creating directory: " << strerror(errno) << std::endl;
        send_response(sock, "ERROR", "Unable to create directory.");
    }
}


void handle_delete(int sock, const std::string &filename) {
    if (filename.empty()) {
        send_response(sock, "ERROR", "File name not specified.");
        return;
    }

    struct stat file_stat;
    if (stat(filename.c_str(), &file_stat) == 0 && S_ISDIR(file_stat.st_mode)) {
        send_response(sock, "ERROR", "Specified path is a directory, not a file.");
        return;
    }

    if (!file_exists(filename)) {
        send_response(sock, "ERROR", "404 - File not found.");
        return;
    }

    if (remove_file(filename)) {
        send_response(sock, "SUCCESS", "File deleted.");
    } else {
        std::cerr << "Error deleting file " << strerror(errno) << std::endl;
        send_response(sock, "ERROR", "Unable to delete file.");
    }
}


void handle_cd(int sock, const std::string &directory) {
    if (directory.empty()) {
        send_response(sock, "ERROR", "Directory not specified.");
        return;
    }

    struct stat dir_stat;
    if (stat(directory.c_str(), &dir_stat) != 0) {
        send_response(sock, "ERROR", "Directory not found.");
        return;
    }

    if (!S_ISDIR(dir_stat.st_mode)) {
        send_response(sock, "ERROR", "Specified path is not a directory.");
        return;
    }

    if (chdir(directory.c_str()) == 0) {
        send_response(sock, "Directory changed.");
    } else {
        std::cerr << "Error changing directory: " << strerror(errno) << std::endl;
        send_response(sock, "ERROR", "Unable to change directory.");
    }
}


void handle_ls(int sock) {
    DIR *dir = opendir(".");
    if (dir == nullptr) {
        std::cerr << "Error opening directory: " << strerror(errno) << std::endl;
        send_response(sock, "ERROR", "Unable to open directory.");
        return;
    }

    struct dirent *entry;
    std::string file_list;
    while ((entry = readdir(dir)) != nullptr) {
        std::string name(entry->d_name);
        if (name != "." && name != ".."){
            file_list += entry -> d_name;
            file_list += "\n";
        }
    }

    closedir(dir);

    if (file_list.empty()) {
        send_response(sock, "Directory is empty.");
        return;
    }

    send_response(sock, file_list);
}


void handle_pwd(int sock) {
    char cwd[BUFFER_SIZE];
    if (getcwd(cwd, sizeof(cwd)) != nullptr) {
        send_response(sock, std::string(cwd));
    } else {
        std::cerr << "Error retrieving current directory: " << strerror(errno) << std::endl;
        send_response(sock, "ERROR", "Unable to retrieve current directory.");
    }
}


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