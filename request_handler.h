#ifndef REQUEST_HANDLER_H
#define REQUEST_HANDLER_H

void handle_request(int client_socket);
void serve_html(int client_socket);
void serve_css(int client_socket);
void handle_get_request(int client_socket, const char* request);
void handle_post_request(int client_socket, const char* request);
void send_404(int client_socket);

#endif // REQUEST_HANDLER_H
