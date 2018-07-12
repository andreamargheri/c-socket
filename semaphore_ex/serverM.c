#include "server.h"
//data on client, file, etc. for each thread
sharingTr_data thread_data[NUM_THREAD];

const char* commands[] = {"!register", "!share", "!list", "!get","!quit"};
const char* err_get[] = {"ERR-SHARED","ERR-FOUND","ERR-SK"};

//Declaring main method for thread
void* sharing_thread (void* thread_data);

//mutex for threads (activated when new connection is accepted)
pthread_mutex_t t_mutex[NUM_THREAD];
//mutex for threads files list
pthread_mutex_t files_mutex[NUM_THREAD];


//number of threads available
//TODO REMOVE!!!!
sem_t *sem_n;
//sem_t sem_n;


//entry point
int main(int argc,char * argv[]){
	struct sockaddr_in serv_addr,client_address;
	int sk = 0, porta = 0, t;
	
	//threads
	pthread_t threads[NUM_THREAD];
	pthread_attr_t p_attr;

	if (argc != 3){
		//print sintassi corretta
		printf("ERRORE! Necessari due argomenti!\n");
		printf("Sintassi: %s <local ip address> <local port> \n\n", argv[0]);
		return -1;
	}

	/* Converto a intero il nome della porta */
	porta = atoi(argv[2]);
	/* Creo il socket (PF_INET) TCP (SOCK_STREAM) */
	sk = socket(PF_INET, SOCK_STREAM, 0);
	if (sk == -1){
		printf("Impossibile aprire il Socket\n");
		return -1;
	}

	int k = 1;
	if (setsockopt(sk, SOL_SOCKET, SO_REUSEADDR, &k, sizeof(k)) == -1){
		printf ("SERVER: Errore nel settaggio socket.\n");
		return 0;
	}

	//dati socket server
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(porta); 

	if (bind(sk, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) == -1){
		printf ("ERRORE SERVER: Impossibile eseguire il bind della socket.\n");
		return 0;
	}

	if (listen (sk, NUM_THREAD) == -1){
		printf("ERRORE SERVER: Impossibile creare coda delle connessioni di dimensione %d", NUM_THREAD);
		return 0;
	}
	printf("Sharing Server Pronto\nIn attesa di connessioni sulla porta %d\n", porta);

	//initialize semaphore for #threads
	//TODO REMOVE!!!!!!!!!
	if ((sem_n = sem_open("/semaphore", O_CREAT, 0644, NUM_THREAD)) == SEM_FAILED){
			printf("AHH!\n");
	}
	
	//if (sem_init(&sem_n, 0, NUM_THREAD)==-1){	
	//	printf("MAIN: errore inizializzazione sincronizzazione server\n %s \n",strerror(errno));
	//	return 0;
	//}

	//define NUM_THREAD threads
	//initialization of PThread attributes
	pthread_attr_init(&p_attr);	
	//threads no joinable
	pthread_attr_setdetachstate(&p_attr, PTHREAD_CREATE_DETACHED);
	for (t =  0; t< NUM_THREAD; t++){
		//set thread num	
		thread_data[t].th_id = t;
		thread_data[t].state = FREE;	
		//initialize mutex
		pthread_mutex_init(&t_mutex[t], NULL);
		//mutex for the file list
		pthread_mutex_init(&files_mutex[t],NULL);
		//block mutex to intially halt the thread
		pthread_mutex_lock(&t_mutex[t]);
		
		/*
		* 1 - pthread's array
		* 2 - phtread attributes
		* 3 - thread's method (sharing routine)
		* 4 - thread's argument (client sk, etc.)
		*/
		pthread_create(&threads[t], &p_attr, sharing_thread, (void*) &thread_data[t]);
		
	}
	int c_sk;
	//Infinite Loop waiting for client connection
	//The accept is completed even if no thread is available, then the flow is halted on the sem_wait($sem)
	while(1){	
		//ADD INFORMATION ON THE CLIENT SOCKET IN THE THREAD DATA STRUCTURE
		socklen_t len = sizeof(client_address);	
		
		//???????????????????????????????????????????????????????????????????????????????????????????
		//perche'?!?!?!?!?!??!?!
		//sem_wait(&sem_n);
		//sem_post(&sem_n);
		
		c_sk = accept(sk,(struct sockaddr*)&(client_address), &len);
		if (c_sk == -1){
			printf("Impossible completare la Accept()\n");
			continue;
		}else{
			/*Accept OK dal client */
			char address[100];
			//Converto indirizzo Client
			inet_ntop(AF_INET, &(client_address), address, sizeof(address));
			printf("MAIN: connessione stabilita con il client %s\n",address);
			//waiting for available thread, then take "signal" one more thread as busy

			//TODO REMOVE!!!
			sem_wait(sem_n);
			//sem_wait(&sem_n);
			//lookinf for a free thread
			int free =0;
			for (free = 0; free<(NUM_THREAD-1); free++){
				if(thread_data[free].state == FREE){
					break;
				}
			}	
			//add information about client socket into th_data
			thread_data[free].sk = c_sk;
			thread_data[free].state = BUSY;
			thread_data[free].client_addr = client_address;
			thread_data[free].n_file = 0;
			thread_data[free].state_file = NO_SHARED;
			thread_data[free].head_files = malloc(sizeof(fl_list*));
			
			/*
			//initialize client_files struct
			files_init(&(thread_data[free].files));
			*/
			
			printf("MAIN: è stato selezionato il thread %d per il client %s\n" , free, address);
			//unlock the assigned thread
			pthread_mutex_unlock(&t_mutex[free]);
		}
	}
}

//MAIN METHOD FOR THREADs
void* sharing_thread(void* thread_data){
	
	//explicit cast for thread_data
	sharingTr_data* this_data = (sharingTr_data*) thread_data;
	
	int ret; 
	char mess[BUF_SIZE];
	int th_id = this_data->th_id;
	
	printf("Thread %d in esecuzione! In attesa di rischieste...\n", th_id);
	
	while(1){
		//lock: waiting for new client connection (acceptance == unlock)
		//next lock is avaialble only when the Main performs a new signal
		pthread_mutex_lock(&t_mutex[th_id]);
		//send acknowledgement of acceptant (ack) to client
		strcpy(mess,"ACK");
		//get socket form thread data
		if(send(this_data->sk, mess, BUF_SIZE ,0) < BUF_SIZE){
			printf("THREAD %d: Errore Invio ACK benvenuto\n",th_id);
			close_client(this_data);
			continue;
		}	
		
		//receiving client username
		if(register_user(this_data) == -1) {
			printf("THREAD %d: Errore impostazione username", th_id);
			close_client(this_data);
			continue;
		}
		
		printf("THREAD %d: Utente %s connesso\n",th_id,this_data->client_user);
		//Commands
		while (1){
			//ricevo comando da client
			ret = recv(this_data->sk, mess, BUF_SIZE, MSG_WAITALL); 		
			if( (ret == -1) || (ret<BUF_SIZE) ) {
				printf("THREAD %d: Errore ricezione comando da %s\n", th_id, this_data->client_user);
				//client goes down without quit command
				close_client(this_data);
				break;
			}	
			//compare mess with the available commands
			if (strcmp(mess,commands[0])== 0){
				//!register
				printf("THREAD %d: ricezione comando !register da %s\n", th_id, this_data->client_user);
				register_file(this_data);
			} else if (strcmp(mess,commands[1])== 0){
				//!share
				printf("THREAD %d: ricezione comando !share da %s\n", th_id, this_data->client_user);
				//thread goes to the modality share/get management
				share(this_data);
			} else if(strcmp(mess, commands[2])== 0){
				//!list
				printf("THREAD %d: ricezione comando !list da %s\n", th_id,this_data->client_user);
				file_list(this_data);			
			}else if (strcmp(mess, commands[3])== 0){
				//!get
				printf("THREAD %d: ricezione comando !get da %s\n", th_id, this_data->client_user);
				file_get(this_data);
			} else if (strcmp(mess, commands[4])== 0){
				//!quit
				printf("THREAD %d: ricezione comando !quit da %s\n", th_id, this_data->client_user);
				//close connection with client
				close_client(this_data);
				break;
			}else {
				//otherwise
				printf("Comando %s non riconosciuto. In attesa di nuovo comando\n", mess);
			}	
		}
		
	}
	pthread_exit(NULL);
}
	
//Check usernamne (unique)
int register_user(sharingTr_data* this_data){ 
	char mess[BUF_SIZE],user_name[BUF_SIZE];
	int th_id = this_data->th_id;
	int ret,i;

	while(1){	
		//receiving client username
		ret = recv(this_data->sk, mess, BUF_SIZE, MSG_WAITALL); 
		if( (ret == -1) || (ret<BUF_SIZE) ) {
			printf("THREAD %d: Errore ricezione username", th_id);
			ret = -1;
			break;
		}
		strcpy(user_name,mess);
		printf("THREAD %d: ricevuto username %s per il client\n",th_id,user_name);
		
		//TODO MUTEX ???????????????????????
		
		//check if usename is available (within mess)
		int flag = 0;
		for (i = 0; i<NUM_THREAD; i++){
			// return 0 if equal
			if (strcmp(thread_data[i].client_user,user_name)==0){
				flag = 1;
				break;
			}
		}
		if (flag==0){
			printf("THREAD %d: username non in uso\n", th_id);
			//username available
			//send OK to client
			strcpy(mess,"USER-OK");
			if(send(this_data->sk, mess, BUF_SIZE ,0) < BUF_SIZE){
				printf("THREAD %d: Errore Invio conferma user\n",th_id);
				ret = -1;
				break;
			}	
			//add client username
		   	strcpy(this_data->client_user, user_name);
			ret = 1;
			break;
		}else{
			//send USER-USED to client (then waiting for new username)
			printf("THREAD %d: username gia' in uso. In attesa nuovo username...\n", th_id);
			strcpy(mess,"USER-USED");
			if(send(this_data->sk, mess, BUF_SIZE ,0) < BUF_SIZE){
				printf("THREAD %d: Errore Invio richiesta nuovo user\n",th_id);
				continue;
			}	
			
		}
	}
	return ret;
}	

//METHODS FOR SENDER CLIENTs	
int register_file(sharingTr_data* this_data){
	char mess[BUF_SIZE];
	int th_id = this_data->th_id;
	int num,i,ret;
	//receiving number of files
	ret = recv(this_data->sk, mess, BUF_SIZE, MSG_WAITALL); 
	if( (ret == -1) || (ret<BUF_SIZE) ) {
		printf("THREAD %d: Errore ricezione numero file", th_id);
		ret = -1;
		return -1;
	}
	//convert string to numeber
	num = atoi(mess);
	printf("THREAD %d: Numero file da ricevere %d\n", th_id,num);
	for (i = 0; i<num; i++){
		//receiving files name
		ret = recv(this_data->sk, mess, BUF_SIZE, MSG_WAITALL); 
		
		if( (ret == -1) || (ret<BUF_SIZE) ) {
			printf("THREAD %d: Errore ricezione nome file\n", th_id);
			ret = -1;
			break;
		}	
		printf("THREAD %d: %s ha registrato file %s - %d di %d \n", th_id,this_data->client_user,mess,i+1,num);
				
		//TODO mutex !!!!!!!! ??????????   ?????????????
		//take lock on the files_list
		pthread_mutex_lock(&files_mutex[th_id]);
		
		//add files to files_list
		files_addFile(this_data, mess);

		//release lock on the files_list		
		pthread_mutex_unlock(&files_mutex[th_id]);
		
		//TODO mutex !!!!!!!! ??????????   ?????????????

	}
	
	return 0;
}

int share(sharingTr_data* this_data){
	char mess[BUF_SIZE];
	int th_id = this_data->th_id;
	printf("THREAD %d: L'utente %s passato in modalita' SHARE\n",this_data->th_id,this_data->client_user);
	this_data->state_file = SHARED;
		
	strcpy(mess,"SHARE OK");
	//get socket form thread data
	if(send(this_data->sk, mess, BUF_SIZE ,0) < BUF_SIZE){
		printf("THREAD %d: Errore Invio conferma SHARE\n",th_id);
		close_client(this_data);
		return -1;
	}	
	
	
		
		//TODO qui si deve mettere un semaforo che si blocca quando e' in share senno il comando viene preso dal corpo principale del thread e non dal comando get
	
	//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!	
	//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	//DEVE ESSERE DIVERSO!!!! NON PUO' ESSERE QUELLO PER LEGGERE I FILE SENNO CHI LO SBLOCCA POI?!?!?!
	//LASCIO WHILE (1){{} LA FARO' IO EH 
	//pthread_mutex_lock(&files_mutex[th_id]);
	//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!	
	//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
	
		//quando passa significa e' stata fatta una signal dal metodo !get (causa sender non piu' disponibile)
		// e quindi si esce liberando la memoria di questo thread che torna disponibile
		
	while(1){
		
	}
	
}

//METHODS FOR RECEIVER CLIENTs
int file_list(sharingTr_data* this_data){
	char mess[BUF_SIZE];
	int th_id = this_data->th_id;
	int n_file = 0, i;
	fl_list* tmp;
	fl_list* tmp1;


	//TODO mutex ????????????? sulla lettura delle informazioni dei thread!!!????????

	
	//resulting list
	fl_list* list = malloc(sizeof(fl_list*));	
	
	for (i=0; i<NUM_THREAD; i++){
		if (thread_data[i].n_file > 0){
			//the i-thread has some registered files
			tmp = thread_data[i].head_files;
			while(tmp != NULL){
				//copy file name
				if (list->file_name == NULL){
					//empty list
					list->file_name = tmp->file_name;
					n_file = n_file + 1;
				}else{
					//check if a file with the same name is already in the list
					int flag = 0;
					tmp1 = list;
					while(tmp1 != NULL){
						if (strcmp(tmp->file_name,tmp1->file_name)==0){
							flag = 1;
							break;
						}
						tmp1 = tmp1->next;
					}
					if (flag == 0){
						//file name not found
						fl_list* new_elem = malloc(sizeof(fl_list*));
						new_elem->file_name = tmp->file_name;
						new_elem->next = list;
						list = new_elem;
						//update total number of files	
						n_file = n_file + 1;
					}					
				}
				//skip to the next elem
				tmp = tmp->next;
			}
		}
	}
	
	//TODO mutex list_files ????????
		
	//send list file to the receiver client
	//first: send the number of files (n_file)
	sprintf(mess, "%d", n_file);
	//get socket form thread data
	if(send(this_data->sk, mess, BUF_SIZE ,0) < BUF_SIZE){
		printf("THREAD %d: Errore Invio elementi LIST\n",th_id);
		close_client(this_data);
		return -1;
	}
	if (list->file_name != NULL){
		//iterate: send each file (list)
		tmp1 = list;
		//here we halt by checking the NULL pointer, in the receiver we use n_file, but it is the same by construction of n_file
		while(tmp1 != NULL){
			//send file name
			strcpy(mess,tmp1->file_name);
			if(send(this_data->sk, mess, BUF_SIZE ,0) < BUF_SIZE){
				printf("THREAD %d: Errore Invio elementi LIST\n",th_id);
				close_client(this_data);
				return -1;
			}
			//skip to the next file
			tmp1 = tmp1->next;
		}
	}
	return 0;
}

int file_get(sharingTr_data* this_data){
	int ret,i, sk_send, len;
	int th_id = this_data->th_id;
	char filename[BUF_SIZE], *tmp_file;
	char resp[BUF_SIZE];
	
	fl_list* tmp;
	
	//receiving the name of the file to search and send
	ret = recv(this_data->sk, filename, BUF_SIZE, MSG_WAITALL); 
	if( (ret == -1) || (ret<BUF_SIZE) ) {
		printf("THREAD %d: Errore ricezione file name", th_id);
		return -1;
	}
	
	//TODO mutex ??????????????????
	
	//looking for the file
	//mess = file name
	
	//status of finding file: 
	// 0 - searching
	// 1 - file found and thread SHARED
	// 2 - file found but thread NO_SHARED
	// 3 - file not found
	int status = 0; 
	//num thread file of the sender
	int thread_file_sender;

	for (i = 0; i<NUM_THREAD; i++){
		if (thread_data[i].n_file > 0){
			//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
			//SE NON PRENDI IL LOCK QUI NON DEVI METTERE LA UNLOCK IN FONDO!!!!!!!!!!!!!!!!!
			pthread_mutex_lock(&files_mutex[i]);
			//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
			//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
			//the i-thread has some registered files
			tmp = thread_data[i].head_files;
			int flag = 0;
			while(tmp != NULL){
				if (strcmp(tmp->file_name,filename) == 0){
					flag = 1;
					break;
				}
				//skip to next
				tmp = tmp->next;
			}
			if (flag == 1){
				//file name found in the i_thread
				//check if the status is SHARED
				if (thread_data[i].state_file == SHARED){
					//file found and thread SHARED
					status = 1;
					thread_file_sender = i;
				}else{
					//file found but thread NO_SHARED
					status = 2;
				}
			} else {
				//file not found in the i_thread
				status = 3;
			}
			pthread_mutex_unlock(&files_mutex[i]);
		}
		if (status == 1){
			//file found thus stop searching
			break;
		}
	
	}
	
	if (status == 1){
		//start receiving / sendining 
		//send err-sk in case of error problem with the sender communication
		printf("THREAD %d: sender %s trovato per condividere file %s con receiver %s \n",th_id, thread_data[i].client_user,filename,this_data->client_user);
		//socket of sender
		sk_send = thread_data[i].sk;
		//switch to no_shared for this thread
		thread_data[i].state_file = NO_SHARED;
		
		
		//TODO attenzione a riabilitare SHARE lo state del sender
		

		//send to receiver the sender name (otherwise in the else branch is sent the error code)
		strcpy(resp,thread_data[i].client_user);
		if(send(this_data->sk, resp, BUF_SIZE ,0) < BUF_SIZE){
			printf("THREAD %d: Errore invio messaggio receiver\n",th_id);
			//reabilitate the SHARED state of the sender
			thread_data[i].state_file = SHARED;	
			//close socket connection with receiver due to a communication error
			close_client(this_data);
			return -1;
		}
		//send to the sender the receiver name
		strcpy(resp,this_data->client_user);
		if(send(sk_send, resp, BUF_SIZE ,0) < BUF_SIZE){
			printf("THREAD %d: Errore invio messaggio sender\n",th_id);
			//send err_get[2] to the receiver
			strcpy(resp,err_get[2]);			
			if(send(this_data->sk, resp, BUF_SIZE ,0) < BUF_SIZE){
				printf("THREAD %d: Errore invio messaggio receiver\n",th_id);
				close_client(this_data);
			}
			//close sender
			close_client(&thread_data[i]);
			
			//TODO SIGNAL TO SHARE METHOD?
			//pthread_mutex_unlock(&files_mutex[i]);
			//???????????????????????????????????????????????????????????????????????????????
			//???????????????????????????????????????????????????????????????????????????????
			//???????????????????????????????????????????????????????????????????????????????
			
			return -1;
		}
		//send to the sender the file name
		strcpy(resp,filename);
		if(send(sk_send, resp, BUF_SIZE ,0) < BUF_SIZE){
			printf("THREAD %d: Errore invio messaggio sender\n",th_id);
			//send err_get[2] to the receiver
			strcpy(resp,err_get[2]);			
			if(send(this_data->sk, resp, BUF_SIZE ,0) < BUF_SIZE){
				printf("THREAD %d: Errore invio messaggio receiver\n",th_id);
				close_client(this_data);
			}
			//close sender
			close_client(&thread_data[i]);
			
			//TODO SIGNAL TO SHARE METHOD?
			//pthread_mutex_unlock(&files_mutex[i]);
			//???????????????????????????????????????????????????????????????????????????????
			//???????????????????????????????????????????????????????????????????????????????
			//???????????????????????????????????????????????????????????????????????????????
			
			return -1;
		}
		//receive file dimension from the sender (or the error err_get[2] if the file is not found)
		ret = recv(sk_send, resp, BUF_SIZE, MSG_WAITALL); 
		if( (ret == -1) || (ret<BUF_SIZE) ) {
			printf("THREAD %d: Errore ricezione dimensione file da sender", th_id);
			//send err_get[2] to the receiver
			strcpy(resp,err_get[2]);			
			if(send(this_data->sk, resp, BUF_SIZE ,0) < BUF_SIZE){
				printf("THREAD %d: Errore invio messaggio receiver\n",th_id);
				close_client(this_data);
			}
			//close sender
			close_client(&thread_data[i]);
			
			//TODO SIGNAL TO SHARE METHOD?
			//pthread_mutex_unlock(&files_mutex[i]);
			//???????????????????????????????????????????????????????????????????????????????
			//???????????????????????????????????????????????????????????????????????????????
			//???????????????????????????????????????????????????????????????????????????????
			
			return -1;
		}
		if (strcmp(resp,err_get[2]) == 0){
			printf("THREAD %d: Sender %s non dispone del file %s. Downlaod interrotto\n", th_id, thread_data[i].client_user,filename);
			//send err_get[2] to the receiver
			strcpy(resp,err_get[2]);			
			if(send(this_data->sk, resp, BUF_SIZE ,0) < BUF_SIZE){
				printf("THREAD %d: Errore invio messaggio receiver\n",th_id);
				close_client(this_data);
			}
			//here we do not close the socket, we reset the state to share, because the error can depend by the file and not by the socket
			//close_client(sk_send);
			thread_data[i].state_file = SHARED;	
			return -1;
		}
		//send dimension to the receiver
		len = atoi(resp);
		tmp_file = (char*) malloc(sizeof(char) * len);
		sprintf(resp, "%d", len);
		if(send(this_data->sk, resp , BUF_SIZE ,0) < BUF_SIZE){
			printf("THREAD %d: Errore invio messaggio receiver\n",th_id);
			//reabilitate the SHARED state of the sender
			thread_data[i].state_file = SHARED;	
			//close socket connection with receiver due to a communication error
			close_client(this_data);
			return -1;
		}
	
		printf("THREAD %d: %s inizia trasferimento di %s a %s\n",th_id,thread_data[i].client_user,filename,this_data->client_user);	
		//receive the file from the sender
		if(recv(sk_send, tmp_file, sizeof(char) * len, MSG_WAITALL) < (sizeof(char) * len)){
			printf("THREAD %d: Errore ricezione file da sender", th_id);
			//send err_get[2] to the receiver
			strcpy(resp,err_get[2]);			
			if(send(this_data->sk, resp, BUF_SIZE ,0) < BUF_SIZE){
				printf("THREAD %d: Errore invio messaggio receiver\n",th_id);
				close_client(this_data);
			}
			//close sender
			close_client(&thread_data[i]);
			
			//TODO SIGNAL TO SHARE METHOD?
			
			//???????????????????????????????????????????????????????????????????????????????
			//pthread_mutex_unlock(&files_mutex[i]);
			//???????????????????????????????????????????????????????????????????????????????
			//DEVE ESSERE UN MUTEX DIVERSO!!!!!!!!!!!!!!!!!!!!!!!
			
			free(tmp_file);

			//TODO FAR RIPARTIRE IL THREAD CHE GESTIVA IL SENDER CHE E' ANDATO GIU

			//???????????????????????????????????????????????????????????????????????????????
			//pthread_mutex_unlock(&t_mutex[i]);
			//???????????????????????????????????????????????????????????????????????????????
			//DEVE ESSERE UN MUTEX DIVERSO!!!!!!!!!!!!!!!!!!!!!!!
			
			
			return -1;
		}
		//send file to the receiver
		if(send(this_data->sk, tmp_file , sizeof(char) * len ,0) < (sizeof(char) * len)){
			printf("THREAD %d: Errore invio file al receiver\n",th_id);
			//reabilitate the SHARED state of the sender
			thread_data[i].state_file = SHARED;	
			//close socket connection with receiver due to a communication error
			close_client(this_data);
			free(tmp_file);
			return -1;
		}
		//return to the share state
		thread_data[i].state_file = SHARED;	
		free(tmp_file);

		printf("THREAD %d: %s completato trasferimento di %s a %s\n",th_id,thread_data[i].client_user,filename,this_data->client_user);				
		
	}else{
		printf("THREAD %d: impossible inviare il file %s a %s. ",th_id,filename,this_data->client_user);				
		//send error to receiver
		if (status == 2){
			//file found but thread NO_SHARED
			strcpy(resp,err_get[0]);
			printf("Sender %s non disponibile\n", thread_data[i].client_user);
		}else {
			//file not found
			strcpy(resp,err_get[1]);			
			printf("Sender %s non ha trovato il file\n", thread_data[i].client_user);
		}
		if(send(this_data->sk, resp, BUF_SIZE ,0) < BUF_SIZE){
			printf("THREAD %d: Errore invio messaggio receiver\n",th_id);
			//reabilitate the SHARED state of the sender
			thread_data[i].state_file = SHARED;	
			close_client(this_data);
			return -1;
		}
	}
	return 0;
}

//CLOSE CONNECTIONs
void close_client(sharingTr_data* thread_data){
	printf("THREAD %d: L'utente %s si è disconnesso\n", thread_data->th_id,thread_data->client_user);
	//chiude il socket
	close(thread_data->sk);
	printf("THREAD %d: Chiusa connessione con client %s\n", thread_data->th_id, thread_data->client_user);
	//clear the username array
	memset(thread_data->client_user, 0, sizeof(thread_data->client_user));
	thread_data->state = FREE;		
	
	
	//add a new free thread
	//TODO REMOVE!!!!!
	sem_post(sem_n);
	//sem_post(&sem_n);
	
	//reset file information
	thread_data->n_file = 0;
	thread_data->state_file = NO_SHARED;
	memset(thread_data->head_files,0,sizeof(fl_list));
}