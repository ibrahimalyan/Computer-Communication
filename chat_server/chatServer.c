#include "chatServer.h"

#include <stdio.h>

#include <stdlib.h>

#include <string.h>

#include <unistd.h>

#include <signal.h>

#include <sys/socket.h>

#include <arpa/inet.h>

#include <sys/ioctl.h>

#include <sys/select.h>
#include <ctype.h>

#define max_port_value 65536
#define BUFFER 4096


static int end_server = 0;

int the_port_is_number(const char *isdigit);
void updateMaxfdInPool(conn_pool_t *pool, int server_socket);
void intHandler(int SIG_INT) {
    end_server = 1;

    if (SIG_INT == SIGINT) {
        return;
    }

    /* use a flag to end_server to break the main loop */
}

int main (int argc, char *argv[])
{

    //checks if recieved wrong parameter.
    if(argc!=2){
        printf("Usage: server <port>");
        exit(EXIT_FAILURE);
    }

    if(the_port_is_number(argv[1]) == -1){
        printf("Usage: server <port>");
        exit(EXIT_FAILURE);
    }


    int port = (int)strtol(argv[1], NULL, 10);
    if(!(port > 0 && port <= max_port_value)){
        printf("Usage: server <port>");
        exit(EXIT_FAILURE);
    }

    signal(SIGINT, intHandler);


    // Initialize connection pool
    conn_pool_t *pool = malloc(sizeof(conn_pool_t));
    if (initPool(pool) == -1) {
        perror("Failed to initialize connection pool");
        exit(EXIT_FAILURE);
    }

    // Create socket
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set socket to non-blocking
    int enable = 1;
    if (ioctl(server_socket, FIONBIO, &enable) == -1) {
        perror("Failed to set non-blocking mode");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // Bind socket
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) == -1) {
        perror("Bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    // Listen
    if (listen(server_socket, SOMAXCONN) == -1) {
        perror("Listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }


    /*************************************************************/
    /* Initialize fd_sets                                         */
    /*************************************************************/
    FD_SET(server_socket,&pool->read_set);
    /*************************************************************/
    /* Loop waiting for incoming connects, for incoming data or  */
    /* to write data, on any of the connected sockets.           */
    /*************************************************************/
    do
    {

        updateMaxfdInPool(pool, server_socket);

        /**********************************************************/
        /* Copy the master fd_set over to the working fd_set.     */
        /**********************************************************/
        pool->ready_write_set=pool->write_set;
        pool->ready_read_set=pool->read_set;
        /**********************************************************/
        /* Call select()                                */
        /**********************************************************/


        printf("Waiting on select()...\nMaxFd %d\n", pool->maxfd);
        pool->nready=select(pool->maxfd+1,&pool->ready_read_set,&pool->ready_write_set,NULL,NULL);
        if(pool->nready<0){
            break;
        }
        /**********************************************************/
        /* One or more descriptors are readable or writable.      */
        /* Need to determine which ones they are.                 */
        /**********************************************************/
        int sd;
        int maxfd_plus_one = pool->maxfd + 1;
        int nready = pool->nready;


        for (sd = 0; sd < maxfd_plus_one && nready > 0; sd++) {
            /* Each time a ready descriptor is found, one less has  */
            /* to be looked for.  This is being done so that we     */
            /* can stop looking at the working set once we have     */
            /* found all of the descriptors that were ready         */

            /*******************************************************/
            /* Check to see if this descriptor is ready for read   */
            /*******************************************************/
            if (FD_ISSET(sd, &pool->ready_read_set)) {

                /***************************************************/
                /* A descriptor was found that was readable          */
                /* if this is the listening socket, accept one      */
                /* incoming connection that is queued up on the     */
                /*  listening socket before we loop back and call   */
                /* select again.                              */
                /****************************************************/
                int *new_server = NULL; // Declare a pointer to int
                if (sd == server_socket) {
                    new_server = (int *)malloc(sizeof(int)); // Allocate memory for new_server
                    if (new_server == NULL) {
                        perror("malloc");
                        break; // Exit the loop if malloc fails
                    }
                    struct sockaddr_in cli;
                    socklen_t cli_len = sizeof(cli);
                    *new_server = accept(sd, (struct sockaddr*)&cli, &cli_len);
                    if (*new_server < 0) {
                        perror("accept");
                        free(new_server); // Free memory if accept fails
                        break; // Exit the loop if accept fails
                    }
                    printf("New incoming connection on sd %d\n", *new_server);
                    nready--;
                    addConn(*new_server, pool);
                }
                    /****************************************************/
                    /* If this is not the listening socket, an           */
                    /* existing connection must be readable             */
                    /* Receive incoming data his socket             */
                    /****************************************************/

                else {
                    printf("Descriptor %d is readable\n", sd);
                    char buffer[BUFFER];
                    int bytes_received = read(sd, buffer, BUFFER_SIZE);

                    if (bytes_received == 0) {
                        printf("Connection closed for sd %d\n", sd);
                        removeConn(sd, pool);
                    }
                    /**********************************************/
                    /* Data was received, add msg to all other    */
                    /* connectios                         */
                    /**********************************************/
                    else {
                        printf("%d bytes received from sd %d\n", bytes_received, sd);
                        nready--;
                        addMsg(sd, buffer, bytes_received, pool);
                    }
                }
                free(new_server); // Free memory after usage


            }
             /* End of if (FD_ISSET()) */
            /*******************************************************/
            /* Check to see if this descriptor is ready for write  */
            /*******************************************************/
            if (FD_ISSET(sd, &pool->ready_write_set)) {
                writeToClient(sd, pool);
                nready--;
            }
        }
            /*******************************************************/
         /* End of loop through selectable descriptors */

    } while (end_server == 0);

    /*************************************************************/
    /* If we are here, Control-C was typed,                    */
    /* clean up all open connections                        */
    /*************************************************************/

    int i = 0;
    int num_connections = pool->nr_conns;
    int *remove = (int*)malloc(num_connections * sizeof(int)); // Allocate memory for the array dynamically

// Traverse the connection list and store file descriptors to remove later
    for (conn_t *current = pool->conn_head; current != NULL; current = current->next) {
        remove[i++] = current->fd;
    }

// Remove connections from the pool in reverse order
    for (i = num_connections - 1; i >= 0; i--) {
        printf("removing connection with sd %d \n", remove[i]); // Add printf statement here

        removeConn(remove[i], pool);
    }

    free(remove); // Free the dynamically allocated memory




    return 0;
}

int initPool(conn_pool_t *pool) {
    if (pool == NULL) return -1;

    pool->maxfd = -1;
    pool->nready = 0;
    FD_ZERO(&pool->read_set);
    FD_ZERO(&pool->ready_read_set);
    FD_ZERO(&pool->write_set);
    FD_ZERO(&pool->ready_write_set);
    pool->conn_head = NULL;
    pool->nr_conns = 0;

    return 0;
}
int addConn(int sd, conn_pool_t *pool) {
    if (pool == NULL) return -1;

    conn_t *new_conn = malloc(sizeof(conn_t));
    if (new_conn == NULL) return -1;

    new_conn->fd = sd;
    new_conn->write_msg_head = NULL;
    new_conn->write_msg_tail = NULL;
    new_conn->prev = NULL;
    new_conn->next = pool->conn_head;
    if (pool->conn_head != NULL)
        pool->conn_head->prev = new_conn;
    pool->conn_head = new_conn;

    pool->nr_conns++;

    if (sd > pool->maxfd)
        pool->maxfd = sd;

    FD_SET(sd, &pool->read_set);

    return 0;
}

int removeConn(int sd, conn_pool_t *pool) {
    if (pool == NULL) return -1;

    conn_t *curr_conn = pool->conn_head;
    while (curr_conn != NULL) {
        if (curr_conn->fd == sd) {
            if (curr_conn->prev != NULL)
                curr_conn->prev->next = curr_conn->next;
            if (curr_conn->next != NULL)
                curr_conn->next->prev = curr_conn->prev;
            if (pool->conn_head == curr_conn)
                pool->conn_head = curr_conn->next;
            close(curr_conn->fd);
            free(curr_conn);
            pool->nr_conns--;
            FD_CLR(sd, &pool->read_set);
            return 0;
        }
        curr_conn = curr_conn->next;
    }

    return -1; // Connection not found
}

int addMsg(int sd, char *buffer, int len, conn_pool_t *pool) {
    if (pool == NULL) return -1;

    conn_t *curr_conn = pool->conn_head;
    while (curr_conn != NULL) {
        if (curr_conn->fd != sd) {
            msg_t *new_msg = malloc(sizeof(msg_t));
            if (new_msg == NULL) return -1;

            new_msg->message = malloc(len);
            if (new_msg->message == NULL) {
                free(new_msg);
                return -1;
            }

            memcpy(new_msg->message, buffer, len);
            new_msg->size = len;
            new_msg->prev = NULL;
            new_msg->next = curr_conn->write_msg_head;

            if (curr_conn->write_msg_head != NULL)
                curr_conn->write_msg_head->prev = new_msg;
            curr_conn->write_msg_head = new_msg;
            if (curr_conn->write_msg_tail == NULL)
                curr_conn->write_msg_tail = new_msg;

            FD_SET(curr_conn->fd, &pool->write_set);
        }
        curr_conn = curr_conn->next;
    }

    return 0;
}

int writeToClient(int sd, conn_pool_t *pool) {
    if (pool == NULL) return -1;

    conn_t *curr_conn = pool->conn_head;
    while (curr_conn != NULL) {
        if (curr_conn->fd == sd) {
            msg_t *curr_msg = curr_conn->write_msg_head;
            while (curr_msg != NULL) {
                for(int i = 0; i < curr_msg->size; i++) {
                    curr_msg->message[i] = toupper(curr_msg->message[i]);
                }
                ssize_t bytes_sent = send(curr_conn->fd, curr_msg->message, curr_msg->size, 0);
                if (bytes_sent == -1) {
                    perror("Send failed");
                    return -1;
                }
                msg_t *temp = curr_msg;
                curr_msg = curr_msg->next;
                free(temp->message);
                free(temp);
            }
            curr_conn->write_msg_head = NULL;
            curr_conn->write_msg_tail = NULL;
            FD_CLR(sd, &pool->write_set);
            return 0;
        }
        curr_conn = curr_conn->next;
    }

    return -1; // Connection not found
}

int the_port_is_number(const char *isdigit) {
    while (*isdigit != '\0') {
        if (*isdigit < '0' || *isdigit > '9') {
            return -1;
        }
        isdigit++;
    }
    return 0;
}

void updateMaxfdInPool(conn_pool_t *pool, int server_socket) {
    int max_fd = server_socket;

    conn_t *curr = pool->conn_head;
    while (curr != NULL) {
        if (curr->fd > max_fd) {
            max_fd = curr->fd;
        }
        curr = curr->next;
    }

    pool->maxfd = max_fd;
}

