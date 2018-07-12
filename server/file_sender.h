#define FILE_SENDER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define BUF_SIZE 1024
#define NUM_THREAD 5

#define BUSY 0
#define FREE 1

int set_username(int sk);
void getListFunctions();
int quit(int sk);
int files_register(int sk);
int files_share(int sk);
 
