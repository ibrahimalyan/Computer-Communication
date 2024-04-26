#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/stat.h>
#include <stdbool.h>



#define URL_SIZE 1024

void parse_url(const char *url, char *protocol, char *host, int *port, char *filepath) {
    const char *ptr;

    // Find the protocol
    ptr = strstr(url, "://");
    if (ptr != NULL) {
        size_t length = ptr - url;
        strncpy(protocol, url, length);
        protocol[length] = '\0';  // Ensure null-termination
        url = ptr + 3;  // Move to the next part of the URL
    } else {
        // No protocol specified, assuming "http"
        printf("Usage: cproxy <URL> [-s]");
        exit(EXIT_FAILURE);
    }

    // Check if "http" is assumed
    if (strcmp(protocol, "http") != 0) {
        printf("Usage: cproxy <URL> [-s]");
        exit(EXIT_FAILURE);
    }

// Find the host and port
    ptr = strchr(url, '/');

    if (ptr != NULL) {
        size_t length = ptr - url;
        const char *host_port = url;

        // Check if port is specified in the host
        const char *port_delimiter = strchr(host_port, ':');
        if (port_delimiter != NULL && port_delimiter < ptr) {
            // Port is specified in the host
            size_t host_length = port_delimiter - host_port;
            strncpy(host, host_port, host_length);
            host[host_length] = '\0';  // Ensure null-termination

            // Extract and convert port to integer
            *port = atoi(port_delimiter + 1);

            // Check if conversion was successful
            if (*port == 0 && port_delimiter[1] != '0') {
                printf("Usage: cproxy <URL> [-s]");
                exit(EXIT_FAILURE);
            }


        } else {
            // Port is not specified, use default port 80
            *port = 80;
            strncpy(host, host_port, length);
            host[length] = '\0';  // Ensure null-termination
        }

    } else {
        // No forward slash found

        // Check if a colon is present in the URL
        const char *colon_delimiter = strchr(url, ':');

        if (colon_delimiter != NULL) {
            // Colon is present, take the host without the colon
            size_t host_length = colon_delimiter - url;
            strncpy(host, url, host_length);
            host[host_length] = '\0';  // Ensure null-termination

            // Extract and convert port to integer
            *port = atoi(colon_delimiter + 1);

            // Check if conversion was successful
            if (*port == 0 && colon_delimiter[1] != '0') {
                printf("Usage: cproxy <URL> [-s]");
                exit(EXIT_FAILURE);
            }
            *port = 80;

        } else {
            // Assume port 80
            *port = 80;
            strcpy(host, url);
        }
    }

    // Find the filepath
    ptr = strchr(url, '/');
    if (ptr != NULL) {
        strcpy(filepath, ptr);
    } else {
        // No filepath specified
        strcpy(filepath, "/");
    }
}

bool file_exists(const char *filename) {
    // Check if the file exists and the user has read access to it
    if (access(filename, F_OK) != -1) {
        return true;
    } else {
        return false;
    }
}



int count_slashes(const char *str) {
    int count = 0;
    while (*str) {
        if (*str == '/')
            count++;
        str++;
    }
    return count;
}

void send_request(const char *host, int port, const char *filepath) {
    int sockfd; // File descriptor for the socket
    struct hostent* server_info = NULL;

    // Create a socket with the address format of IPV4 over TCP
    if ((sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Use gethostbyname to translate host name to network byte order ip address
    server_info = gethostbyname(host);
    if (!server_info) {
        herror("gethostbyname failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Initialize sockaddr_in structure
    struct sockaddr_in sock_info;

    // Set its attributes to 0 to avoid undefined behavior
    memset(&sock_info, 0, sizeof(struct sockaddr_in));

    // Set the type of the address to be IPV4
    sock_info.sin_family = AF_INET;

    // Set the socket's port
    sock_info.sin_port = htons(port);

    // Set the socket's ip
    sock_info.sin_addr.s_addr = ((struct in_addr *)server_info->h_addr)->s_addr;

    // Connect to the server
    if (connect(sockfd, (struct sockaddr *)&sock_info, sizeof(struct sockaddr_in)) == -1) {
        perror("connect failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Construct the HTTP request
    char request[1024];


    snprintf(request, sizeof(request), "GET %s HTTP/1.0\r\nHost: %s\r\n\r\n", filepath, host);

    int len =strlen(request);

    // Print the constructed request
    printf("HTTP request =\n%s\nLEN = %d\n", request,len );



    // Send the request
    send(sockfd, request, strlen(request), 0);



    char folder_path[URL_SIZE];
    //char *folder_path
    char *token;
    int slash_count = 0;

    // Create the root folder
    mkdir(host, 0777);

    // Tokenize the filepath using '/'
    memset(folder_path,0,sizeof (char));

    strcpy(folder_path, host);
    int slashes_count = count_slashes(filepath);

    token = strtok((char *) filepath, "/");

    while (token != NULL) {
        // Append the current token to the folder path
        strcat(folder_path, "/");
        strcat(folder_path, token);

        slash_count++;

        // Check if it's the last token
        char *next_token = strtok(NULL, "/");

        if (next_token == NULL && slash_count==slashes_count) {

            // If it's the last token, break the loop
            break;
        }

        // Create the folder
        mkdir(folder_path, 0777);

        // Move to the next token
        token = next_token;

    }
    if (slash_count!=slashes_count){

        strcat(folder_path, "/");
    }

    // Check if the last token is present and does not end with '/'
    if (folder_path[strlen(folder_path) - 1] == '/' ) {
        strcat(folder_path, "index.html");
    }

    if ( strchr(folder_path, '/') == NULL) {
        strcat(folder_path, "/index.html");
    }


    FILE *indexFile = fopen(folder_path,"w");
    if(indexFile == NULL){
        perror("error");
        exit(1);
    }



// Receive and display the response
    char storage[1000];
    ssize_t bytes_received;
    ssize_t Content_Length = 0;
    int clean_header = 0;
    int boolFirstTime = 0;
    while ((bytes_received = recv(sockfd, storage, 1000 - 1, 0)) > 0) {
        fwrite(storage, sizeof(char), bytes_received, stdout); // Write to stdout
        // Check if "200 OK" is present in the response
        if (boolFirstTime == 0 && strstr(storage, "200 OK") == NULL) {
            // Delete the file
            fclose(indexFile); // Close the file before deletion
            remove(folder_path); // Delete the file
            close(sockfd);
            exit(0);
        }
        boolFirstTime = 1;

        if (!clean_header) {
            // Find the end of the header
            char *endHeader = strstr(storage, "\r\n\r\n");
            if (endHeader != NULL) {
                clean_header = 1;
                fwrite(endHeader + 4, sizeof(char), bytes_received - (endHeader - storage + 4),
                       indexFile); // Write to file after the header
                Content_Length += bytes_received - (endHeader - storage + 4);
            }
        } else {
            fwrite(storage, sizeof(char), bytes_received, indexFile); // Write to file
            Content_Length += bytes_received;
        }
        
    }

    
// Calculate response length
    size_t response_length = Content_Length;

    // Print the length of the response
    printf("Content-Length: %zu\n", Content_Length);


    // Calculate total response length (body + header)
    fseek(indexFile, 0, SEEK_END);
    printf("\n Total response length: %zu\n", response_length);



// Close the index file
    fclose(indexFile);
    //Close the socket
    close(sockfd);
}

size_t calculate_request_length(const char *filepath) {
    // Find the position of the first '/'
    const char *host_end = strchr(filepath, '/');
    if (host_end == NULL) {
        fprintf(stderr, "Invalid filepath\n");
        exit(EXIT_FAILURE);
    }

    // Calculate the length of the host and filepath
    size_t host_length = host_end - filepath;
    size_t filepath_length = strlen(host_end);

    // Calculate the length of the request
    return strlen("GET ") + filepath_length + strlen(" HTTP/1.0\r\nHost: ") + host_length + strlen("\r\n\r\n");
}
void send_response(const char *filename) {

    if (filename[strlen(filename) - 1] == '/') {
        strcat((char*)filename, "index.html");
    }

    if (strchr(filename, '/') == NULL) {
        strcat((char*)filename, "/index.html");
    }

    // Open the file for reading
    FILE *file = fopen(filename, "r");
    if (file == NULL) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    // Initialize variables to keep track of lengths
    size_t content_length = 0; // Length of the body


    char buffer[1024]; // Buffer to read chunks of the file
    size_t bytes_read; // Variable to hold the number of bytes read

    printf("File is given from local filesystem\n");
    printf("HTTP/1.0 200 OK\r\n");

    // Calculate content length
    fseek(file, 0, SEEK_END); // Move file pointer to the end
    content_length = ftell(file); // Get current position in the file, which is its size
    rewind(file); // Rewind file pointer back to the beginning

    printf("Content-Length: %ld\r\n\r\n", content_length);

    // Read chunks of the file and write them to stdout
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        // Write the read chunk to stdout
        fwrite(buffer, 1, bytes_read, stdout);
    }

    // Close the file
    fclose(file);

    // Calculate the length of the request and add it to content length
    size_t request_length = calculate_request_length(filename);
    size_t total_response_length = content_length + request_length-9;

    // Print the total response length
    printf("\n Total response bytes: %ld\n", total_response_length);
}



int main(int argc, char *argv[]) {

    if (argc != 2) {
        fprintf(stderr, "Usage: cproxy <URL> [-s]\n");
        exit(EXIT_FAILURE);
    }
    char URL[URL_SIZE];
    char protocol[50];
    char host[256];
    int port;
    char filepath[URL_SIZE];

    //  Copy the URL from the command-line argument
    strncpy(URL, argv[1], URL_SIZE - 1);
    URL[URL_SIZE - 1] = '\0';

    parse_url(URL, protocol, host, &port, filepath);

    // Concatenate host and filepath

    char filename[URL_SIZE];
    strcpy(filename, host);
    strcat(filename, filepath);

    //Check if the file exists locally
    if (file_exists(filename)) {
        send_response(filename);
    }

    else {
        send_request(host, port, filepath);
    }

    return 0;
}
