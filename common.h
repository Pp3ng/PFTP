#ifndef FTP_COMMON_H
#define FTP_COMMON_H

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define PORT 9527
#define BUFFER_SIZE 1024
#define MAX_FILENAME 256

// File transfer header structure
typedef struct
{
  char filename[MAX_FILENAME];
  long filesize;
  int transfer_flag; // 0: ready, 1: start, 2: end
} FileHeader;

#endif
