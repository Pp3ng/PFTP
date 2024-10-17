#define _GNU_SOURCE

#include "common.h"
#include <sys/select.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h> // for splice

#define LOG_BUFFER_SIZE 2048 // Log message buffer size

void handle_client(int client_fd, struct sockaddr_in client_addr);
void log_message(const char *message);

int main()
{
  int server_fd, client_fd, max_fd, activity;
  struct sockaddr_in server_addr, client_addr;
  socklen_t client_len = sizeof(client_addr);
  fd_set read_fds;

  // Create socket
  server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0)
  {
    perror("Socket creation failed");
    log_message("Socket creation failed");
    exit(EXIT_FAILURE);
  }

  // Configure server address
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(PORT);

  // Bind socket
  if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
  {
    perror("Bind failed");
    log_message("Bind failed");
    close(server_fd);
    exit(EXIT_FAILURE);
  }

  // Listen for connections
  if (listen(server_fd, 5) < 0)
  {
    perror("Listen failed");
    log_message("Listen failed");
    close(server_fd);
    exit(EXIT_FAILURE);
  }

  printf("FTP Server is running on port %d...\n", PORT);
  log_message("Server started");

  // Initialize the set of active sockets
  FD_ZERO(&read_fds);
  FD_SET(server_fd, &read_fds);
  max_fd = server_fd;

  while (1)
  {
    fd_set temp_fds = read_fds;

    // Wait for activity on one of the sockets
    activity = select(max_fd + 1, &temp_fds, NULL, NULL, NULL);
    if (activity < 0)
    {
      perror("Select error");
      log_message("Select error");
      continue;
    }

    // Check if there is a new connection
    if (FD_ISSET(server_fd, &temp_fds))
    {
      client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
      if (client_fd < 0)
      {
        perror("Accept failed");
        log_message("Accept failed");
        continue;
      }

      char log_msg[LOG_BUFFER_SIZE];
      snprintf(log_msg, LOG_BUFFER_SIZE, "Client connected: %s", inet_ntoa(client_addr.sin_addr));
      printf("%s\n", log_msg);
      log_message(log_msg);

      FD_SET(client_fd, &read_fds);
      if (client_fd > max_fd)
      {
        max_fd = client_fd;
      }
    }

    // Check all clients for data
    for (int i = 0; i <= max_fd; i++)
    {
      if (i != server_fd && FD_ISSET(i, &temp_fds))
      {
        handle_client(i, client_addr);
        FD_CLR(i, &read_fds);
      }
    }
  }

  close(server_fd);
  return EXIT_SUCCESS;
}

void handle_client(int client_fd, struct sockaddr_in client_addr)
{
  FileHeader file_header;
  char log_msg[LOG_BUFFER_SIZE];

  while (1)
  {
    // Receive file header
    ssize_t header_size = read(client_fd, &file_header, sizeof(FileHeader));
    if (header_size <= 0)
    {
      if (header_size < 0)
      {
        perror("Receive header failed");
        log_message("Receive header failed");
      }
      break;
    }

    // Construct the full path to save the file
    char full_path[BUFFER_SIZE];
    snprintf(full_path, BUFFER_SIZE, "%s/%s", getenv("HOME"), "PFTP_FILES");
    mkdir(full_path, 0755); // Create PFTP_FILES directory if it doesn't exist

    // Create directory for the client's IP address
    char client_ip_dir[BUFFER_SIZE];
    snprintf(client_ip_dir, BUFFER_SIZE + 1, "%s/%s", full_path, inet_ntoa(client_addr.sin_addr));
    mkdir(client_ip_dir, 0755);

    // Ensure the filename does not include the full path from the client
    char *relative_path = file_header.filename;
    if (relative_path[0] == '/')
    {
      relative_path++; // Skip the leading '/'
    }

    snprintf(full_path, BUFFER_SIZE + 1, "%s/%s", client_ip_dir, relative_path);

    // Create directories if necessary
    char *dir = strdup(full_path);
    char *last_slash = strrchr(dir, '/');
    if (last_slash != NULL)
    {
      *last_slash = '\0';
      if (strlen(dir) > 0)
      {
        char mkdir_cmd[BUFFER_SIZE];
        snprintf(mkdir_cmd, BUFFER_SIZE, "mkdir -p %s", dir);
        system(mkdir_cmd);
        snprintf(log_msg, LOG_BUFFER_SIZE, "Created directory: %s", dir);
        log_message(log_msg);
      }
    }
    free(dir);

    printf("Receiving file: %s (size: %ld bytes)\n", full_path, file_header.filesize);
    snprintf(log_msg, LOG_BUFFER_SIZE, "Receiving file: %s (size: %ld bytes) from %s", full_path, file_header.filesize, inet_ntoa(client_addr.sin_addr));
    log_message(log_msg);

    // Open file for writing
    int file_fd = open(full_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file_fd < 0)
    {
      perror("File creation failed");
      log_message("File creation failed");
      close(client_fd);
      return;
    }

    // Receive file data
    long remaining = file_header.filesize;
    ssize_t read_size;
    int pipe_fds[2];

    // Create a pipe for transferring data
    if (pipe(pipe_fds) == -1)
    {
      perror("Pipe creation failed");
      log_message("Pipe creation failed");
      close(file_fd);
      close(client_fd);
      return;
    }

    // Use splice to transfer data from the client socket to the file
    while (remaining > 0)
    {
      // Read data from the client socket and write it to the pipe
      read_size = splice(client_fd, NULL, pipe_fds[1], NULL,
                         remaining > BUFFER_SIZE ? BUFFER_SIZE : remaining,
                         SPLICE_F_MOVE | SPLICE_F_MORE); // use splice to transfer data from the client socket to the file,do not need to read data into a buffer then write it to the user space
      if (read_size <= 0)
      {
        perror("Receive data failed");
        log_message("Receive data failed");
        break;
      }

      // Read data from the pipe and write it to the file
      if (splice(pipe_fds[0], NULL, file_fd, NULL, read_size, SPLICE_F_MOVE | SPLICE_F_MORE) != read_size)
      {
        perror("Write data failed");
        log_message("Write data failed");
        break;
      }

      remaining -= read_size;
    }

    close(pipe_fds[0]);
    close(pipe_fds[1]);
    close(file_fd);
    printf("File received successfully: %s\n", full_path);
    snprintf(log_msg, LOG_BUFFER_SIZE, "File received successfully: %s (size: %ld bytes) from %s", full_path, file_header.filesize, inet_ntoa(client_addr.sin_addr));
    log_message(log_msg);
  }

  close(client_fd);
}

void log_message(const char *message)
{
  int log_fd = open("server.log", O_WRONLY | O_CREAT | O_APPEND, 0644);
  if (log_fd != -1)
  {
    time_t now = time(NULL);
    char *time_str = ctime(&now);
    time_str[strlen(time_str) - 1] = '\0'; // Remove newline character
    dprintf(log_fd, "[%s] %s\n", time_str, message);
    close(log_fd);
  }
  else
  {
    perror("Log file open failed");
  }
}