#ifndef MYFTP_H
#define MYFTP_H

#include <thread>
#include <string>


/**
 * @brief Receives a response message from a socket.
 * 
 * This function reads data from the given socket and returns it as a `std::string`.
 * It handles the null-termination of the received data to ensure the response is properly formatted.
 * 
 * @param sock The socket file descriptor from which to receive the message.
 * 
 * @return A `std::string` containing the message received from the socket.
 * 
 * @throws std::runtime_error If the connection is closed or if no data is received.
 * 
 * @note Ensure that the socket is properly connected and initialized before calling this function.
 */
std::string receive_response(int sock);


/**
 * @brief Sends a standardized response to the client.
 * 
 * This function formats the response as "<message>\n" and sends it to the client.
 * It is useful for maintaining consistency in server-client communication.
 * 
 * @param sock The client's socket file descriptor.
 * @param command A command to be issued to the server.
 */ 
void send_command(int sock, const std::string &command);


/**
 * @brief Sends a terminate request to the server.
 * 
 * @param terminate_sock The termination socket file descriptor.
 * @param command_id The ID of the command to terminate.
 */
void send_terminate_request(int terminate_sock, int command_id);


/**
 * @brief Handles the "get" command to download a file from the server.
 * 
 * This function sends a "get <filename>" command to the server, initiates a file transfer,
 * and writes the received data to a local file. It ensures proper handling of the server's
 * response and detects the end of the file transfer using a "FILE_TRANSFER_END" marker.
 * 
 * @param sock The socket file descriptor used for communication with the server.
 * @param filename The name of the file to be downloaded from the server.
 * 
 * @note The function creates a local file with the same name as the requested file.
 *       If the file already exists locally, it will be overwritten.
 * 
 * @warning Ensure the server adheres to the "FILE_TRANSFER_START" and "FILE_TRANSFER_END"
 *          protocol for this function to work correctly.
 * 
 * @details
 * - Sends the command "get <filename>" to the server.
 * - Receives a response from the server to confirm file transfer readiness.
 * - Streams file data from the server until the "FILE_TRANSFER_END" marker is found.
 * - Writes the file data to a binary file with the given filename.
 * - Handles errors such as connection issues or inability to create the local file.
 * 
 * @throws std::runtime_error If there are socket-related issues during communication.
 * 
 * @example
 * @code
 * int sock = connect_to_server();
 * handle_get(sock, "example.txt");
 * @endcode
 */
void handle_get(int sock, const std::string &filename);


/**
 * @brief Handles the "put" command to upload a file to the server.
 * 
 * This function opens a local file, sends a "put <filename>" command to the server, 
 * and transmits the file's content. It ensures the server is ready to receive data 
 * and appends a "FILE_TRANSFER_END" marker to signal the end of the file transfer.
 * 
 * @param sock The socket file descriptor used for communication with the server.
 * @param filename The name of the file to be uploaded to the server.
 * 
 * @note The function expects the server to respond with "SUCCESS: READY_TO_RECEIVE" 
 *       before transmitting the file. If the file does not exist locally or the server 
 *       is not ready, the operation will terminate.
 * 
 * @details
 * - Opens the specified file in binary mode for reading.
 * - Sends the command "put <filename>" to inform the server of the upload request.
 * - Waits for a confirmation response from the server before proceeding.
 * - Reads the file in chunks (using a buffer) and sends each chunk over the socket.
 * - Sends a "FILE_TRANSFER_END" marker to signify the end of the file transfer.
 * - Handles server responses after the transfer to confirm the operation's success.
 * 
 * @warning Ensure that the server implements the "READY_TO_RECEIVE" and "FILE_TRANSFER_END" 
 *          protocol for successful operation.
 * 
 * @throws std::runtime_error If there are socket-related issues during communication.
 * 
 * @example
 * @code
 * int sock = connect_to_server();
 * handle_put(sock, "example.txt");
 * @endcode
 */
void handle_put(int sock, const std::string &filename);


/**
 * @brief Handles the main interactive client loop.
 * 
 * Continuously reads user commands, sends them to the server, and processes responses.
 * Supports file upload ("put"), file download ("get"), and termination ("quit").
 * 
 * @param sock The socket file descriptor for communication with the server.
 */
void client_loop(int sock, int terminate_sock);


/**
 * @brief Establishes a connection to the server.
 * 
 * Resolves the hostname and connects to the specified server using the given port.
 * 
 * @param hostname The server hostname or IP address.
 * @param port The port number to connect to.
 * @param sock Reference to a socket file descriptor that will be initialized upon successful connection.
 * 
 * @throws std::runtime_error If unable to resolve the hostname or connect to the server.
 */
void connect_to_server(const std::string &hostname, int port, int &sock);


/**
 * @brief Entry point of the FTP client program.
 * 
 * Parses command-line arguments for server IP and port, connects to the server, 
 * and starts the interactive client loop.
 * 
 * @param argc Number of command-line arguments.
 * @param argv Array of command-line arguments. Expects <server_ip> and <port>.
 * 
 * @return 0 on successful execution, 1 on failure.
 */
int main(int argc, char *argv[]);

#endif