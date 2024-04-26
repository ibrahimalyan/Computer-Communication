322675356
Ibrahim Alyan
EX3
---Files---
chatServer.h: Header file containing function prototypes and structure definitions.
chatServer.c: Source file implementing the server functionalities.
---Description---
The program implements a simple chat server using the select() function for handling multiple client connections concurrently.
---Functions---
1-initPool: Initializes the connection pool structure.
2-addConn: Adds a new connection to the connection pool.
3-removeConn: Removes a connection from the connection pool.
4-addMsg: Adds a message to be sent to all other connections in the pool.
5-writeToClient: Writes messages to the clients.
6-the_port_is_number: Checks if a given string represents a valid port number.
7-updateMaxfdInPool: Updates the maximum file descriptor in the connection pool.
---How to Use---
1. Compile the program:
gcc -o chatServer chatServer.c
./chatServer <port>
---Output---
The server waits for incoming connections and handles incoming data from clients. It can accept multiple clients concurrently and broadcasts incoming messages to all connected clients.




