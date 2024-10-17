#define _GNU_SOURCE

#include "common.h"
#include <fcntl.h>
#include <sys/sendfile.h>
#include <dirent.h>

void send_file(int sock_fd, const char *filepath);
void send_directory(int sock_fd, const char *dirpath);

int main(int argc, char *argv[])
{
  if (argc < 3 || argc > 4)
  {
    printf("Usage: %s [-r] <server_ip> <filename|directory>\n", argv[0]);
    return EXIT_FAILURE;
  }

  int sock_fd;
  struct sockaddr_in server_addr;
  int recursive = 0;
  const char *server_ip;
  const char *path;

  if (argc == 4 && strcmp(argv[1], "-r") == 0)
  {
    recursive = 1;
    server_ip = argv[2];
    path = argv[3];
  }
  else
  {
    // check if file or directory
    struct stat path_stat;
    if (stat(argv[2], &path_stat) < 0)
    {
      perror("Stat failed");
      return EXIT_FAILURE;
    }
    if (S_ISDIR(path_stat.st_mode))
    {
      printf("Send directory please use -r flag\n");
      exit(EXIT_FAILURE);
    }
    else
    {
      server_ip = argv[1];
      path = argv[2];
    }
  }

  // Create socket
  sock_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (sock_fd < 0)
  {
    perror("Socket creation failed");
    return EXIT_FAILURE;
  }

  // Configure server address
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(PORT);

  if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0)
  {
    perror("Invalid address");
    close(sock_fd);
    return EXIT_FAILURE;
  }

  // Connect to server
  if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
  {
    perror("Connection failed");
    close(sock_fd);
    return EXIT_FAILURE;
  }

  if (recursive)
  {
    send_directory(sock_fd, path);
  }
  else
  {
    send_file(sock_fd, path);
  }

  close(sock_fd);
  return EXIT_SUCCESS;
}

void send_file(int sock_fd, const char *filepath)
{
  int file_fd = open(filepath, O_RDONLY);
  if (file_fd < 0)
  {
    perror("File open failed");
    return;
  }

  // Get file size
  struct stat file_stat;
  if (fstat(file_fd, &file_stat) < 0)
  {
    perror("File stat failed");
    close(file_fd);
    return;
  }

  // Prepare file header
  FileHeader file_header;
  strncpy(file_header.filename, filepath, MAX_FILENAME - 1);
  file_header.filename[MAX_FILENAME - 1] = '\0'; // Ensure null-termination
  file_header.filesize = file_stat.st_size;
  file_header.transfer_flag = 1; // Start transfer

  // Send file header
  if (write(sock_fd, &file_header, sizeof(FileHeader)) < 0)
  {
    perror("Send header failed");
    close(file_fd);
    return;
  }

  printf("Sending file: %s (size: %ld bytes)\n", filepath, file_stat.st_size);

  // Send file data using sendfile
  off_t offset = 0;
  ssize_t sent_bytes;
  while ((sent_bytes = sendfile(sock_fd, file_fd, &offset, file_stat.st_size)) > 0)
  {
    printf("\rProgress: %.2f%%", (float)offset / file_stat.st_size * 100);
    fflush(stdout);
  }

  if (sent_bytes < 0)
  {
    perror("Send data failed");
  }

  printf("\nFile sent successfully\n");

  close(file_fd);
}

void send_directory(int sock_fd, const char *dirpath)
{
  DIR *dir = opendir(dirpath);
  if (!dir)
  {
    perror("Directory open failed");
    return;
  }

  struct dirent *entry;
  char filepath[BUFFER_SIZE];

  while ((entry = readdir(dir)) != NULL)
  {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
    {
      continue;
    }

    snprintf(filepath, BUFFER_SIZE, "%s/%s", dirpath, entry->d_name);

    struct stat entry_stat;
    if (stat(filepath, &entry_stat) < 0)
    {
      perror("Stat failed");
      continue;
    }

    if (S_ISDIR(entry_stat.st_mode))
    {
      send_directory(sock_fd, filepath);
    }
    else if (S_ISREG(entry_stat.st_mode))
    {
      send_file(sock_fd, filepath);
    }
  }

  closedir(dir);
}