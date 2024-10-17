# FTP Server-Client Application

## Project Overview

This is a simple FTP server-client application that allows users to transfer files and directories over the network. The server listens on a specific port and waits for incoming connections from clients. Server can log the details of the files and directories received from the client.Clients use command-line arguments to specify the server IP address and the file or directory to send.

## File Descriptions

- `client.c`: Implements the client functionality, responsible for connecting to the server and sending files or directories.
- `server.c`: Implements the server functionality, responsible for receiving files or directories sent by the client and saving them locally.
- `common.h`: Contains constants and data structure definitions shared between the client and server.
- `.vscode/settings.json`: VS Code configuration file.

## Compilation and Execution

### Compilation

Use the following commands to compile the client and server:

```sh
gcc -o client client.c
gcc -o server server.c
```

### Excution

#### server

Run the following command to start the server:

```sh
./server
```

The server will start listening on port 9527.

#### client

Run the following command on the client to connect to the server and send a file.

```sh
./clinet <server_ip> <file_path>
```

To recurisvely send a directory, use the following command:

```sh
./client -r <server_ip> <directory_path>
```

## Logging

The server logs messages to the `server.log` file, including client connections and file reception details.

Error Handling
Both the client and server print error messages to standard output and log them to the log file when necessary.
