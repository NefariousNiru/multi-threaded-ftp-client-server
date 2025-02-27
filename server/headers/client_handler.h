#ifndef CLIENT_HANDLER_H
#define CLIENT_HANDLER_H

#include <unordered_map>
#include <string>
#include <functional>


/**
 * @brief Handles a single client connection.
 * 
 * @param sock The client's socket file descriptor.
 */
void handle_client(int sock);


/**
 * @brief Parses and executes a command received from the client.
 * 
 * @param command The command string received from the client.
 * @param sock The client's socket file descriptor.
 */
void execute_command(const std::string &command, int sock);


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
CommandMap create_command_map();


/**
 * @brief Prints the current working directory.
 * 
 * @param sock The client's socket file descriptor.
 */
void handle_pwd(int sock);


/**
 * @brief Lists files and directories in the current directory.
 * 
 * @param sock The client's socket file descriptor.
 */
void handle_ls(int sock);


/**
 * @brief Changes the current working directory on the server.
 * 
 * @param sock The client's socket file descriptor.
 * @param directory The target directory to change to.
 */
void handle_cd(int sock, const std::string &directory);


/**
 * @brief Deletes a file from the server's current working directory.
 * 
 * @param sock The client's socket file descriptor.
 * @param filename The name of the file to delete.
 */
void handle_delete(int sock, const std::string &filename);


/**
 * @brief Creates a new directory in the current working directory.
 * 
 * @param sock The client's socket file descriptor.
 * @param directory_name The name of the new directory to create.
 */
void handle_mkdir(int sock, const std::string &directory_name);


/**
 * @brief Sends a file from the server to the client.
 * 
 * @param sock The client's socket file descriptor.
 * @param filename The name of the file to send.
 */
void handle_get(int sock, const std::string &filename);


/**
 * @brief Receives a file from the client and saves it on the server.
 * 
 * @param sock The client's socket file descriptor.
 * @param filename The name of the file to save on the server.
 */
void handle_put(int sock, const std::string &filename);


/**
 * @brief Trims trailing whitespace, including '\n' and '\r'
 * 
 * @param str  String to be cleaned
 */
std::string trim(const std::string &str);


/**
 * @brief Sends a standardized response to the client.
 * 
 * This function formats the response as "<message>\n" and sends it to the client.
 * It is useful for maintaining consistency in server-client communication.
 * 
 * @param sock The client's socket file descriptor.
 * @param message A detailed message providing context or additional information about the status.
 */ 
void send_response(int sock, const std::string &message);


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
void send_response(int sock, const std::string &status, const std::string &message);


/**
 * @brief Sends a standardized response to the client.
 * 
 * Formats the response as "<status>: <message>\n" or "<message>\n" and sends it to the client.
 * Logs an error if the `send` call fails.
 * 
 * @param sock The client's socket file descriptor.
 * @param response The formatted response string to send.
 */
void send_response_impl(int sock, const std::string &response);


/**
 * @brief Removes a file from the filesystem.
 * 
 * @param path The path to the file to remove.
 * @return true if the file was successfully removed, false otherwise.
 */
bool remove_file(const std::string &path);


/**
 * @brief Creates a new directory with 0755 permissions.
 * 
 * @param path The path of the directory to create.
 * @return true if the directory was successfully created, false otherwise.
 */
bool create_directory(const std::string &path);


/**
 * @brief Checks if a file or directory exists.
 * 
 * @param path The path to the file or directory.
 * @return true if the file or directory exists, false otherwise.
 */
bool file_exists(const std::string &path) ;


#endif