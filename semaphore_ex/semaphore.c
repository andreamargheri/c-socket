#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <string.h>
#include <errno.h>

sem_t *sem_n;

int main(int argc,char * argv[]){
	
//	int ret = sem_init(&sem_n,0,5);
	
	if ((sem_n = sem_open("/semaphore1", O_CREAT, 0644, 3)) ==SEM_FAILED){
		printf("AHHHH!\n");
	}
	
	//printf("%d %s\n",ret,strerror(errno));
	int val;
	sem_wait(sem_n);
	printf("1 \n");
	sem_wait(sem_n);
	printf("2\n");	
	sem_wait(sem_n);
	printf("ciao\n");
	
	
	sem_post(sem_n);


	sem_close(sem_n);
	sem_unlink("/semaphore");	
	sem_unlink("/semaphore1");	
	
}
