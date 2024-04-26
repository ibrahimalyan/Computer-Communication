#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include "threadpool.h"


// CLIENT_INFO structure
typedef struct {
    int sockfd;
    struct sockaddr_in sockinfo;
} CLIENT_INFO;

#define FILTER_FILE "filter.txt" // Path to the filter file


// check if the hostname or IP address is within the filter
bool isFiltered(const char* host) {
    FILE *filter_fp;
    char filter_line[100];
    filter_fp = fopen(FILTER_FILE, "r");

    if (filter_fp == NULL) {
        perror("Error opening filter file");
        exit(EXIT_FAILURE);
    }

    while (fgets(filter_line, sizeof(filter_line), filter_fp) != NULL) {
        // Trim newline character
        filter_line[strcspn(filter_line, "\n")] = 0;

        // Check if the filter line matches the host
        if (strcmp(filter_line, host) == 0) {
            fclose(filter_fp);
            return true; // Host matches the filter
        }

        // Check if the filter line is a sub-network
        char* sub_network = strtok(filter_line, "/");
        char* end = strtok(NULL, "/");

        if (sub_network != NULL && end != NULL) {
            // Check if the host matches the sub-network
            struct in_addr addr;
            inet_pton(AF_INET, sub_network, &addr);

            struct in_addr host_addr;
            if (inet_pton(AF_INET, host, &host_addr) == 1) {
                int mask = atoi(end);
                uint32_t network_mask = htonl(~((1 << (32 - mask)) - 1));
                if ((host_addr.s_addr & network_mask) == (addr.s_addr & network_mask)) {
                    fclose(filter_fp);
                    return true; // Host matches the filter
                }
            }
        }
    }

    fclose(filter_fp);
    return false; // Host doesn't match any filter
}
void bad_request(void* arg) {
    char            buf[4096];
    int             bread = 0;
    CLIENT_INFO     cinfo = *(CLIENT_INFO*)arg;

    memset(buf, 0, 4096);




    /* read whatever the client sends */
    bread = read(cinfo.sockfd, buf, 4096);





    /* check whether there was an error during read operation */
    if (bread == -1) { // error
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            printf("Read timeout occurred\n");
        else
            perror("read");
        goto end;
    }

    /* check if the client closed the connection */
    if (bread == 0) {
        goto end;
    }



    char temp_buffer[4096];
    memset(temp_buffer, 0, 4096);

    strcpy(temp_buffer, buf);

    char temp2_buffer[4096];
    memset(temp2_buffer, 0, 4096);

    strcpy(temp2_buffer, buf);

    time_t rawtime;
    struct tm* timeinfo;
    char time_buffer[80];
    time(&rawtime);
    timeinfo = gmtime(&rawtime);
    strftime(time_buffer, sizeof(time_buffer), "%a, %d %b %Y %H:%M:%S GMT", timeinfo);


    // Check if the request contains method, path, and protocol
    char *method = strtok(temp_buffer, " ");
    char *path = strtok(NULL, " ");
    char *protocol = strtok(NULL, "\r\n");

    if (!method || !path || !protocol) {
        // Send 400 Bad Request response
        char *response = "<HTML><HEAD><TITLE>400 Bad Request</TITLE></HEAD>\r\n"
                         "<BODY><H4>400 Bad request</H4>\r\n"
                         "Bad Request.\r\n"
                         "</BODY></HTML>";
        // Calculate content length
        size_t content_length = strlen(response);
        // Construct the error response with timestamp and content length
        char error_response[4096];
        snprintf(error_response, sizeof(error_response),
                 "HTTP/1.1 400 Bad Request\r\n"
                 "Server: webserver/1.0\r\n"
                 "Date: %s\r\n"
                 "Content-Type: text/html\r\n"
                 "Content-Length: %zu\r\n"
                 "Connection: close\r\n"
                 "\r\n"
                 "%s", time_buffer, content_length, response);



        write(cinfo.sockfd, error_response, strlen(error_response));
        goto end;
    }



    // Check if the protocol is one of the supported HTTP versions
    if (strcmp(protocol, "HTTP/1.0") != 0 && strcmp(protocol, "HTTP/1.1") != 0) {
        // Send 400 Bad Request response
        char *response = "<HTML><HEAD><TITLE>400 Bad Request</TITLE></HEAD>\r\n"
                         "<BODY><H4>400 Bad request</H4>\r\n"
                         "Bad Request.\r\n"
                         "</BODY></HTML>";

        // Calculate content length
        size_t content_length = strlen(response);
        // Construct the error response with timestamp and content length
        char error_response[4096];
        snprintf(error_response, sizeof(error_response),
                 "HTTP/1.1 400 Bad Request\r\n"
                 "Server: webserver/1.0\r\n"
                 "Date: %s\r\n"
                 "Content-Type: text/html\r\n"
                 "Content-Length: %zu\r\n"
                 "Connection: close\r\n"
                 "\r\n"
                 "%s", time_buffer, content_length, response);
        write(cinfo.sockfd, error_response, strlen(error_response));
        goto end;
    }




    // Check if the method is GET
    if (strcmp(method, "POST") == 0 || strcmp(method, "DELETE") == 0 || strcmp(method, "PUT") == 0) {

        // Send 501 Not Implemented response
        char *response = "<HTML><HEAD><TITLE>501 Not supported</TITLE></HEAD>\r\n"
                         "<BODY><H4>501 Not supported</H4>\r\n"
                         "Method is not supported.\r\n"
                         "</BODY></HTML>";

        // Calculate content length
        size_t content_length = strlen(response);
        // Construct the error response with timestamp and content length
        char error_response[4096];
        snprintf(error_response, sizeof(error_response),
                 "HTTP/1.1 501 Not supported\r\n"
                 "Server: webserver/1.0\r\n"
                 "Date: %s\r\n"
                 "Content-Type: text/html\r\n"
                 "Content-Length: %zu\r\n"
                 "Connection: close\r\n"
                 "\r\n"
                 "%s", time_buffer, content_length, response);
        write(cinfo.sockfd, error_response, strlen(error_response));
        goto end;
    }



        const char* temp2_host = "Host: ";
        const size_t temp_host_len = strlen(temp2_host);

        const char* temp_host_start = strstr(temp2_buffer, temp2_host);


    temp_host_start += temp_host_len;

        const char* temp_host_end = strchr(temp_host_start, '\r');
        if (temp_host_end == NULL) {
            temp_host_end = temp2_buffer + strlen(temp2_buffer);
        }

        // Calculate the length of the host string
        size_t host_length = temp_host_end - temp_host_start;

        // Allocate memory for the host string and copy the host
        char* HOST = (char*)malloc(host_length + 1); // +1 for null terminator

        strncpy(HOST, temp_host_start, host_length);
    HOST[host_length] = '\0'; // Null-terminate the string

 //    Check if the hostname or IP address is filtered
    if (isFiltered(HOST)) {
        char *response = "<HTML><HEAD><TITLE>403 Forbidden</TITLE></HEAD>\r\n"
                         "<BODY><H4>403 Forbidden</H4>\r\n"
                         "Access denied.\r\n"
                         "</BODY></HTML>";

        // Calculate content length
        size_t content_length = strlen(response);
        // Construct the error response with timestamp and content length
        char error_response[4096];
        snprintf(error_response, sizeof(error_response),
                 "HTTP/1.1 403 Forbidden\r\n"
                 "Server: webserver/1.0\r\n"
                 "Date: %s\r\n"
                 "Content-Type: text/html\r\n"
                 "Content-Length: %zu\r\n"
                 "Connection: close\r\n"
                 "\r\n"
                 "%s", time_buffer, content_length, response);
        write(cinfo.sockfd, error_response, strlen(error_response));
        goto end;

    }





    const char* host_token = "Host: ";
    const size_t host_token_len = strlen(host_token);

    const char* host_start = strstr(buf, host_token);


    host_start += host_token_len;
    const char* host_end = strchr(host_start, '\r');
    if (host_end == NULL) {
        host_end = buf + strlen(buf);
    }

    // Calculate the length of the host string
    size_t host_len = host_end - host_start;

    // Allocate memory for the host string and copy the host
    char* host = (char*)malloc(host_len + 1); // +1 for null terminator

    strncpy(host, host_start, host_len);
    host[host_len] = '\0'; // Null-terminate the string
        // Connect to the server
    int server_sock;
    struct hostent* server_info = NULL;

    if ((server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
        perror("server socket");
        goto end;
    }



    // Use gethostbyname to translate host name to network byte order ip address
    server_info = gethostbyname(host);
    if (!server_info) {
        char *response = "<HTML><HEAD><TITLE>404 Not Found</TITLE></HEAD>\r\n"
                         "<BODY><H4>404 Not Found</H4>\r\n"
                         "File not found.\r\n"
                         "</BODY></HTML>";

        // Calculate content length
        size_t content_length = strlen(response);
        // Construct the error response with timestamp and content length
        char error_response[4096];
        snprintf(error_response, sizeof(error_response),
                 "HTTP/1.1 404 Not Found\r\n"
                 "Server: webserver/1.0\r\n"
                 "Date: %s\r\n"
                 "Content-Type: text/html\r\n"
                 "Content-Length: %zu\r\n"
                 "Connection: close\r\n"
                 "\r\n"
                 "%s", time_buffer, content_length, response);
           write(cinfo.sockfd, error_response, strlen(error_response));
        close(server_sock);
        goto end;
    }
    // Initialize sockaddr_in structure
    struct sockaddr_in sock_info;

    //struct sockaddr_in server_addr;
    // Set its attributes to 0 to avoid undefined behavior
    memset(&sock_info, 0, sizeof(struct sockaddr_in));

    // Set the type of the address to be IPV4
    sock_info.sin_family = AF_INET;



    // Set the socket's port
    sock_info.sin_port = htons(80);    // Set the socket's ip
    // Set the socket's ip
    sock_info.sin_addr.s_addr = ((struct in_addr *)server_info->h_addr)->s_addr;


    // Connect to the server
    if (connect(server_sock, (struct sockaddr *)&sock_info, sizeof(struct sockaddr_in)) == -1) {
        perror("connect failed");
        close(server_sock);
        goto end;

    }


    // Send request to server
    if (write(server_sock, buf, strlen(buf)) < 0) {
        perror("write to server");
        close(server_sock);
        goto end;
    }


    // Read response from server and send back to client
    while ((bread = read(server_sock, buf, strlen(buf))) > 0) {
        if (write(cinfo.sockfd, buf, bread) < 0) {
            perror("write to client");
            break;
        }
    }

    // Close server socket
    close(server_sock);



    end:
    close(cinfo.sockfd);
}
int main(int argc, char* argv[]) {

    if (argc != 5) {
        fprintf(stderr, "Usage: %s <port> <num_of_tasks> <max_tasks> <filter>\n", argv[0]);
        return 1;
    }
    struct sockaddr_in serverinfo;
    int wsock;
    in_port_t port = (in_port_t)strtoul(argv[1], NULL, 10);
    size_t num_of_tasks = strtoul(argv[2], NULL, 10);
    size_t max_tasks = strtoul(argv[3], NULL, 10);

    threadpool* tp = create_threadpool(1);

    num_of_tasks = num_of_tasks > max_tasks ? max_tasks : num_of_tasks;

    memset(&serverinfo, 0, sizeof(struct sockaddr_in));

    serverinfo.sin_family = AF_INET;
    serverinfo.sin_port = htons(port);
    serverinfo.sin_addr.s_addr = inet_addr("127.0.0.1");

    if ((wsock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
        perror("socket");
        return 0;
    }

    if (bind(wsock, (struct sockaddr*)&serverinfo, sizeof(struct sockaddr_in)) == -1) {
        close(wsock);
        perror("bind");
    }

    if (listen(wsock, 5) == -1) {
        close(wsock);
        perror("listen");
    }

    for (size_t i = 0; i < num_of_tasks; i++) {
        CLIENT_INFO* cinfo = (CLIENT_INFO*)malloc(sizeof(CLIENT_INFO));
        socklen_t struct_len = sizeof(struct sockaddr_in);
        cinfo->sockfd = accept(wsock, (struct sockaddr*)&cinfo->sockinfo, &struct_len);
        dispatch(tp, (dispatch_fn)bad_request, (void*)cinfo);
    }

    destroy_threadpool(tp);
    close(wsock);
}