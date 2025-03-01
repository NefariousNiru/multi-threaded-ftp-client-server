#ifndef UTIL_H
#define UTIL_H

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


/**
 * @brief Creates a data socket bound to an ephemeral port.
 *
 * This function creates an IPv6 TCP socket, binds it to an ephemeral port (chosen by the OS),
 * and prepares it for listening. It retrieves the assigned port number and returns it.
 *
 * @param data_sock_out Reference to an integer where the created socket descriptor will be stored.
 * @param ephemeral_port_out Reference to an integer where the assigned ephemeral port will be stored.
 * @return `true` if the socket was successfully created and bound, `false` otherwise.
 */
bool create_data_socket(int &data_sock_out, int &ephemeral_port_out);

#endif