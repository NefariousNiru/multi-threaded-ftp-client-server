#ifndef CLIENT_HANDLER_H
#define CLIENT_HANDLER_H

#include <unordered_map>
#include <string>
#include <functional>


/**
 * @brief A structure to define a client session
 * 
 * @property currPath - Current Path of a Client for a given Session.  
 */
struct ClientSession { std::string currPath; };


/**
 * @brief Joins session path with name
 *
 * @param session Client session.
 * @param name Name to join (file or directory)
 */
std::string get_full_path(ClientSession &session, const std::string &name);


/**
 * @brief Sends a file sent over a socket.
 *
 * This function handles outgoing file transfer from a client. It sends the file
 * and ends with a termination marker ("FILE_TRANSFER_END\n") to mark end of file.
 * If the transfer is aborted, it informs the client.
 *
 * @param sock The socket descriptor for sending data.
 * @param command_ID The unique ID associated with this file transfer command.
 * @param filename The name of the file where the file is stored.
 */
void send_data_get(int sock, int command_ID, const std::string &filename);


/**
 * @brief Receives and saves a file sent over a socket.
 *
 * This function handles incoming file transfer from a client. It writes the received
 * data to the specified file and checks for a termination marker ("FILE_TRANSFER_END\n").
 * If the transfer is aborted, it deletes the incomplete file and informs the client.
 *
 * @param sock The socket descriptor for receiving data.
 * @param command_ID The unique ID associated with this file transfer command.
 * @param filename The name of the file where the received data should be stored.
 */
void receive_data_put(int data_client_sock, int command_ID, const std::string &filename);


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


using CommandFunction = std::function<void(int, const std::string&, ClientSession &)>;
using CommandMap = std::unordered_map<std::string, CommandFunction>;
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

#endif