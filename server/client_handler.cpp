#include "client_handler.h"
#include "util.cpp"
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
extern std::unordered_map<int, int> command_to_client;
extern std::mutex command_mutex;


std::string get_full_path(ClientSession &session, const std::string &name) {
    std::string fullPath = session.currPath;
    if (fullPath.back() != '/') {
        fullPath += '/';
    }
    fullPath += name;
    return fullPath;
}


void send_data_get(int sock, int command_ID, const std::string &filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        send_response(sock, "ERROR", "Unable to open file.");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(command_mutex);
        active_commands[command_ID] = true;
        command_to_client[command_ID] = sock;
    }

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
        command_to_client.erase(command_ID);
    }
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
        command_to_client[command_ID] = sock;
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
        command_to_client.erase(command_ID);
    }

    if (leftover_data.empty()) {
        send_response(sock, "ERROR", "File transfer failed.");
    } else {
        send_response(sock, "SUCCESS", "File transfer completed.");
    }

    close(sock);
}


void handle_put(int sock, const std::string &filename, ClientSession &session) {
    if (filename.empty()) {
        send_response(sock, "ERROR", "File name not specified.");
        return;
    }

    std::string fullPath = get_full_path(session, filename);

    if (file_exists(fullPath)) {
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
        receive_data_put(data_client_sock, command_ID, fullPath);
    });

    t.detach();
}


void handle_get(int sock, const std::string &filename, ClientSession &session) {
    if (filename.empty()) {
        send_response(sock, "ERROR", "File name not specified.");
        return;
    }

    std::string fullPath = get_full_path(session, filename);

    if (!file_exists(fullPath)) {
        send_response(sock, "ERROR", "404 - File not found.");
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

    std::thread t([=](){
        sockaddr_in6 data_client_addr;
        socklen_t len = sizeof(data_client_addr);

        int data_client_sock = accept(data_sock, (sockaddr*)&data_client_addr, &len);
        close(data_sock); // Once we accept connection we dont need to lsiten anymore so close it

        if (data_client_sock < 0) {
            std::cerr << "Failed to accept data connection.\n";
            return;
        }
        send_data_get(data_client_sock, command_ID, fullPath);
    });

    t.detach();
}


void handle_mkdir(int sock, const std::string &directory_name, ClientSession &session) {
    if (directory_name.empty()) {
        send_response(sock, "ERROR", "Directory name not specified.");
        return;
    }

    std::string fullPath = get_full_path(session, directory_name);

    struct stat path_stat;
    if (stat(fullPath.c_str(), &path_stat) == 0) {
        if (S_ISDIR(path_stat.st_mode)) {
            send_response(sock, "ERROR", "Directory already exists.");
        } else {
            send_response(sock, "ERROR", "A file with the same name exists.");
        }
        return;
    }

    if (create_directory(fullPath)) {
        send_response(sock, "SUCCESS", "Directory created successfully.");
    } else {
        std::cerr << "Error creating directory: " << strerror(errno) << std::endl;
        send_response(sock, "ERROR", "Unable to create directory.");
    }
}


void handle_delete(int sock, const std::string &filename, ClientSession &session) {
    if (filename.empty()) {
        send_response(sock, "ERROR", "File name not specified.");
        return;
    }

    std::string fullPath = get_full_path(session, filename);

    struct stat file_stat;
    if (stat(fullPath.c_str(), &file_stat) == 0 && S_ISDIR(file_stat.st_mode)) {
        send_response(sock, "ERROR", "Specified path is a directory, not a file.");
        return;
    }

    if (!file_exists(fullPath)) {
        send_response(sock, "ERROR", "404 - File not found.");
        return;
    }

    if (remove_file(fullPath)) {
        send_response(sock, "SUCCESS", "File deleted.");
    } else {
        std::cerr << "Error deleting file: " << strerror(errno) << std::endl;
        send_response(sock, "ERROR", "Unable to delete file.");
    }
}


void handle_cd(int sock, const std::string &directory, ClientSession &session) {
    if (directory.empty()) {
        send_response(sock, "ERROR", "Directory not specified.");
        return;
    }

    std::string newPath;
    
    // Handle absolute path
    if (directory[0] == '/') {
        newPath = directory;
    } 
    // Handle relative paths
    else {
        // Handle ".." (go up one directory)
        if (directory == "..") {
            size_t lastSlash = session.currPath.find_last_of('/');
            if (lastSlash == 0) {
                // We're at root directory
                newPath = "/";
            } else if (lastSlash != std::string::npos) {
                newPath = session.currPath.substr(0, lastSlash);
            } else {
                newPath = session.currPath; // Stay in current directory if path is malformed
            }
        } 
        // Handle "." (current directory)
        else if (directory == ".") {
            newPath = session.currPath;
        } 
        // Handle other relative paths
        else {
            // Ensure path ends with '/' for proper concatenation
            if (session.currPath.back() != '/') {
                newPath = session.currPath + "/" + directory;
            } else {
                newPath = session.currPath + directory;
            }
        }
    }

    // Normalize path to ensure it doesn't have trailing slash (except for root)
    if (newPath.length() > 1 && newPath.back() == '/') {
        newPath.pop_back();
    }

    // Check if new path exists and is a directory
    struct stat dir_stat;
    if (stat(newPath.c_str(), &dir_stat) != 0) {
        send_response(sock, "ERROR", "Directory not found.");
        return;
    }

    if (!S_ISDIR(dir_stat.st_mode)) {
        send_response(sock, "ERROR", "Specified path is not a directory.");
        return;
    }

    // Update the session's current path
    session.currPath = newPath;
    send_response(sock, "OK", "Directory changed to " + session.currPath);
}


void handle_ls(int sock, ClientSession &session) {
    DIR *dir = opendir(session.currPath.c_str());
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


void handle_pwd(int sock, ClientSession &session) {
    send_response(sock, session.currPath);
}


CommandMap create_command_map() {
    CommandMap command_map;

    // Commands without arguments
    command_map["pwd"] = [](int sock, const std::string &, ClientSession &session) { handle_pwd(sock, session); };
    command_map["ls"] = [](int sock, const std::string &, ClientSession &session) { handle_ls(sock, session); };

    // Commands with arguments
    command_map["cd"] = [](int sock, const std::string &arg, ClientSession &session) { handle_cd(sock, arg, session); };
    command_map["mkdir"] = [](int sock, const std::string &arg, ClientSession &session) { handle_mkdir(sock, arg, session); };
    command_map["delete"] = [](int sock, const std::string &arg, ClientSession &session) { handle_delete(sock, arg, session); };
    command_map["get"] = [](int sock, const std::string &arg, ClientSession &session) { handle_get(sock, arg, session); };
    command_map["put"] = [](int sock, const std::string &arg, ClientSession &session) { handle_put(sock, arg, session); };

    return command_map;
}


void execute_command(const std::string &command, int sock, ClientSession &session) {
    // Create the command map
    static CommandMap command_map = create_command_map();

    // Parse the command and argument
    size_t space_pos = command.find(' ');
    std::string cmd = (space_pos == std::string::npos) ? command : command.substr(0, space_pos);
    std::string arg = (space_pos == std::string::npos) ? "" : trim(command.substr(space_pos + 1));

    // Find the command in the map
    auto it = command_map.find(cmd);
    if (it != command_map.end()) {
        it->second(sock, arg, session);
    } else {
        send_response(sock, "ERROR", "Invalid command.");
    }
}


void handle_client(int sock) {
    const char *welcome_msg = "\033[32mConnected to MyFTPServer!\033[0m";
    send_response(sock, welcome_msg);

    ClientSession session;
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        session.currPath = cwd;
    } else {
        perror("getcwd failed");
        session.currPath = "./";
    }

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

        execute_command(command, sock, session);
    }

    close(sock);
}