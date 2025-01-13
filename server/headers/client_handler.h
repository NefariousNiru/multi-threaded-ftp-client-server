#ifndef CLIENT_HANDLER_H
#define CLIENT_HANDLER_H

#include <string>

void handle_client(int sock);
void handle_pwd(int sock);
void handle_ls(int sock);

#endif