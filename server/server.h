#define SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <pthread.h>
#include <semaphore.h>

#define BUF_SIZE 1024
#define NUM_THREAD 5

#define BUSY 0
#define FREE 1

#define SHARED 1
#define NO_SHARED 0

/*
//shared files
typedef struct{
	//file list (reference to the head)
	fl_list* head_list;
}client_file;
*/


//list for files
typedef struct fl_list_t{
	char* file_name;
	struct fl_list_t* next;
}fl_list;

//client data
typedef struct{
	//thread_id allocated to client
	int th_id;
	int sk;
	//BUSY or FREE thread
	int state;
	struct sockaddr_in client_addr;
	//client username
	char client_user[BUF_SIZE];
	//files number
	int n_file;
	//SHARED o NO_SHARED (files)
	int state_file;
	//list of files
	fl_list* head_files;
}sharingTr_data;

/*
//init file list 
void files_init(fl_list* files){
	
	//TODO MUTEX ???????????????
	
	files->n_file = 0;
	//files->state = NO_SHARED;
	files->head_list = malloc(sizeof(fl_list*));
	
}*/

//add file
void files_addFile(sharingTr_data* this_data, char* name){
	char *tmp = malloc (1 + strlen (name));
	//check if empty list
	if (this_data->n_file == 0){
		//empty list
		strcpy(tmp,name);
		this_data->head_files->file_name = tmp;
		this_data->head_files->next = NULL;
	}else{
		//add new element as head element
		fl_list* new_elem = malloc(sizeof(fl_list*));

//		printf("PRIMA COPY - %s\n", this_data->head_files->file_name);

		strcpy(tmp,name);
		new_elem->file_name = tmp;
		new_elem->next = this_data->head_files;

//		printf("PRIMA DOPO COPY - %s\n", this_data->head_files->file_name);
//		printf("PRIMA - %s\n", new_elem->file_name);
//		printf("PRIMA - %s\n", new_elem->next->file_name);

		//update the head
		this_data->head_files = new_elem;
	}

//	printf("%s\n", this_data->head_files->file_name);
//	if (this_data->head_files->next != NULL){
//		printf("%s\n", this_data->head_files->next->file_name);
//	}
	
	//update counter
	this_data->n_file = this_data->n_file + 1;
//	printf("Counter %d\n", this_data->n_file);
}


void close_client(sharingTr_data* thread_data);
int register_user(sharingTr_data* thread_data);	
//METHODS FOR SENDER CLIENTs	
int register_file(sharingTr_data* thread_data);
int share(sharingTr_data* thread_data);
//METHODS FOR RECEIVER CLIENTs
int file_list(sharingTr_data* thread_data);
int file_get(sharingTr_data* thread_data);

