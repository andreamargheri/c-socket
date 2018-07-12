#include "file_sender.h"

const char* commands[] = {"!help","!register","!share","!quit"}; 
const char* err_get[] = {"ERR-SHARED","ERR-FOUND","ERR-SK"};

int scanner(const char *input, char *buff_return, size_t buflen);

//entry point
int main(int argc,char * argv[]){
	
	struct sockaddr_in server_addr;
	int sk,porta, ret;

	char mess[BUF_SIZE],input[BUF_SIZE];

	if (argc != 3){
		//print sintassi corretta
		printf("ERRORE! Necessari due argomenti!\n");
		printf("Sintassi: %s <local ip address> <local port> \n\n", argv[0]);
		return -1;
	}
	
	porta = atoi(argv[2]);
	/* Creazione del socket : int socket(int domain, int type, int protocol); */
	sk = socket(PF_INET, SOCK_STREAM, 0);
	
	if (sk == -1){
		printf("Impossibile aprire il Socket\n");
		return -1;
	}
	
	//reset data strct
	memset(&server_addr,0,sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(argv[1]);
	server_addr.sin_port = htons(porta);
	
	if (inet_pton(AF_INET,argv[1],&server_addr.sin_addr) <= 0){
		//address not valid
		printf("Indirizzo %s:%d non valido!\n", argv[1],porta);	
		return -1;
	}
	ret = connect(sk,(struct sockaddr*) &server_addr,sizeof(server_addr));
	if (ret == -1){
		printf("Errore nella connessione con il server %s:%d\n",argv[1],porta);
		close(sk);
		return -1;
	}
	printf("In attesa di risposta (ACK) dal server...\n");
	ret = recv(sk, mess, BUF_SIZE, MSG_WAITALL); 
	if( (ret == -1) || (ret<BUF_SIZE) ) {
		printf("ERRORE: impossibile connettersi al server %s:%d\n",argv[1],porta);
		close(sk);
		return -1;
	}
	
	//strcmp: return 0 when equal (NB 0 == false)
	if (strcmp(mess,"ACK")==1){
		printf("Errore nella ricezione ACK dal server %s:%d\n",argv[1],porta);
		close(sk);
		return -1;
	}
		
	printf("Connessione al server %s (porta %d) effettuata con successo!\n\n",argv[1],porta);
	
	if (set_username(sk)==-1){
		//errore
		printf("Errore nell'impostazione del username con il server %s:%d. Connessione chiusa.\n",argv[1],porta);
		close(sk);
		return -1;
	}
	//set username ok
	printf("\n");
	getListFunctions();

	while(1){
		printf("Inserisci comando: ");
		scanf("%s",input);
		scanner(input, mess, BUF_SIZE);
	
		//compare mess with the available commands
		if (strcmp(mess,commands[0])== 0){
			//!help
			getListFunctions();
		} else if(strcmp(mess, commands[1])== 0){
			//!register
			files_register(sk);
		}else if (strcmp(mess, commands[2])== 0){
			//!share
			files_share(sk);
		} else if (strcmp(mess, commands[3])== 0){
			//!quit
			quit(sk);
			return -1;
		}else {
			//otherwise
			printf("Comando %s  non disponibile. Riprova!\n", mess);
		}
	}	
}


void getListFunctions(){
	printf("Sono disponibili i seguenti comandi:\n");
	printf("*\t!help --> Mostra l'elenco dei comandi disponibili\n");
	printf("*\t!register --> Notifica al server quali file condividere\n");
	printf("*\t!share --> Condividi i file (attendi richieste da servire)\n");
	printf("*\t!quit --> Disconnetti dal server e chiudi il programma\n\n");
}

//return:
// 1  - username correct
// -1 - error
int set_username(int sk){ 
	char input[BUF_SIZE],mess[BUF_SIZE];
	int ret; 
		
	while(1){
		printf("Inserisci il nome utente: ");
		scanf("%s",input);
		//scanning input
		scanner(input, mess, BUF_SIZE);
		//send username to server
		if (send(sk, mess, BUF_SIZE ,0)< BUF_SIZE){
			printf("Errore invio username. Riprova\n");
			ret = -1;
			break;
		} 
		//wait for response (USER-OK | USER-USED)
		ret = recv(sk, mess, BUF_SIZE, MSG_WAITALL); 
		if( (ret == -1) || (ret<BUF_SIZE) ) {
			printf("Errore ricezione conferma dal server\n");
			ret = -1;
			break;
		}
		//strcmp: return 0 when equal (NB 0 == false)
		if (strcmp(mess,"USER-USED")==0){
			printf("Username gia' in uso. Riprova\n");
			continue;
		}else if (strcmp(mess,"USER-OK")==0){
			//username available
			printf("Username impostato correttamente\n");
			ret = 1;
			break;
		}else {
			printf("ERRORE: Comunicazione non riconosciuta!\n");
			ret = -1;
			break;
		}
	}
	return ret;
}


int files_register(int sk){
	//int ret;
	char mess[BUF_SIZE];
	char tmp[BUF_SIZE];
	char *try_read;
	FILE *tmp_file;
	int i,num,file_dim;
	
	while(1){
		//insert number of files to register
		printf("Quanti file vuoi condividere?\n");
		scanf("%s",tmp);
		num = atoi(tmp);	
		if (num <= 0){
			printf("Inserisci numero maggiore (o uguale) a 1. Riprova.\n");
			continue;
		}
		break;
	}
	strcpy(mess,commands[1]);
	printf("Invio comando register...\n");
	if(send(sk, mess, BUF_SIZE ,0) < BUF_SIZE){
		printf("Errore Invio register\n");
		return -1;
	}	
	//send number of files to register
	strcpy(mess,tmp);
	if(send(sk, mess, BUF_SIZE ,0) < BUF_SIZE){
		printf("Errore Invio informazioni files\n");
		return -1;
	}
	printf("Inserisci il nome dei file da condividere:\n");
	//num files
	for (i = 0; i<num; i++){
		
		while(1){
			char file_dir[BUF_SIZE]= "files_s/";		
			printf("(%d,%d) ",i+1,num);
			//tmp file name
			scanf("%s",tmp);
			//tmp = input
			//mess = output
			scanner(tmp, mess, BUF_SIZE);
			//files are in the files/ folder
			strcat(file_dir,mess);
			//printf("FILE OPEN %s\n",file_dir);
			//check if file exists
			tmp_file = fopen(file_dir,"r");
			if ( tmp_file != NULL){
				//read all the file
				fseek(tmp_file,0,SEEK_END);
				file_dim = ftell(tmp_file);
				fseek(tmp_file,0,SEEK_SET);
				try_read = (char*) malloc(sizeof(char)*file_dim);
				if (fread(try_read,sizeof(char),file_dim,tmp_file)!= file_dim){	
					free(try_read);
					printf("Impossibile leggere il file %s\n",tmp);
				} else {	
					//file exites and can be read -> send file name
					free(try_read);
					break;
				}
			} else {
				//otherwise retry		
				printf("Il file scelto non esiste. Riprova\n");
			}
		}
		//send name of the chosen file
		if(send(sk, mess, BUF_SIZE ,0) < BUF_SIZE){
			printf("Errore Invio informazioni files\n");
			return -1;
		}
	}
	return 0;
}


int files_share(int sk){
	int ret,file_size;
	char mess[BUF_SIZE], *tmp;
	char filename[BUF_SIZE];
	FILE *file_tosend;
	
	printf("Invio comando share...\n");
	if(send(sk, commands[2], BUF_SIZE ,0) < BUF_SIZE){
		printf("Errore Invio SHARE\n");
		return -1;
	}	
	
	ret = recv(sk, mess, BUF_SIZE, MSG_WAITALL); 
	if( (ret == -1) || (ret<BUF_SIZE) ) {
		printf("ERRORE: impossibile ricevere messaggio conferma share\n");
		close(sk);
		return -1;
	}
	//strcmp: return 0 when equal (NB 0 == false)
	if (strcmp(mess,"SHARE OK")==1){
		printf("Errore: impossibile ricevere messaggio conferma share\n");
		//close connection
		quit(sk);
		return -1;
	}
	//mess ok -> waiting for file request 
	printf("Modalita' SHARE confermata.");
	
	/*
	*
	*MODICATO - INIZIO
	*	
	*/
	
	//while(1){
	
	/*
	*
	*MODICATO - FINE
	*	
	*/
	
	char file_path[BUF_SIZE]= "files_s/";
	//lock waiting for new receiver ask to the current thread a file
	printf("In attesa di richieste file...\n");
	//receive sender name
	ret = recv(sk, mess, BUF_SIZE, MSG_WAITALL); 
	if( (ret == -1) || (ret<BUF_SIZE) ) {
		printf("ERRORE: impossibile ricevere messaggio conferma share\n");
		close(sk);
		return -1;
	}
	//receive file name
	ret = recv(sk, filename, BUF_SIZE, MSG_WAITALL); 
	if( (ret == -1) || (ret<BUF_SIZE) ) {
		printf("ERRORE: impossibile ricevere messaggio conferma share\n");
		close(sk);
		return -1;
	}
	printf("File Receiver %s richiede file %s\n",mess,filename);
	printf("Trasferimento del file %s in corso...\n",filename);
	//looking for the file (first the file dimension)
	strcat(file_path,filename);
	file_tosend = fopen(file_path,"r");

	if (file_tosend == NULL){
		//file cannot be opened
		printf("Impossibile aprire il file %s!\n",filename);
		
		//send error to server
		if(send(sk, err_get[2] , BUF_SIZE ,0) < BUF_SIZE){
			//socket error close
			printf("ERRORE: impossibile inviare messaggio server\n");
			close(sk);
			return -1;
		}
		return 0;
	}else{
		//file can be opened
		fseek(file_tosend,0,SEEK_END);
		//get file dimension
		file_size = ftell(file_tosend);
		fseek(file_tosend,0,SEEK_SET);
	}
	
	sprintf(mess,"%d",file_size);
	//send file dimension to the server
	if(send(sk, mess, BUF_SIZE ,0) < BUF_SIZE){
		//socket error close
		printf("ERRORE: impossibile inviare messaggio server\n");
		close(sk);
		return -1;
	}
	//allocate space for reading file
	tmp = (char*) malloc(sizeof(char)*file_size);
	if (fread (tmp,sizeof(char),file_size,file_tosend)!= file_size){	
		printf("Impossibile leggere il file %s\n",tmp);
		return 0;
	}
	if(send(sk, tmp , file_size ,0) < file_size){
		//socket error close
		printf("ERRORE: impossibile inviare messaggio server\n");
		close(sk);
		return -1;
	}
	printf("Trasferimento del file %s terminato!\n",filename);
	/*
	*
	*MODICATO - INIZIO
	*	
	*/
	//}
	//receive sharing end
	ret = recv(sk, mess, BUF_SIZE, MSG_WAITALL); 
	if( (ret == -1) || (ret<BUF_SIZE) ) {
		printf("ERRORE: impossibile ricevere messaggio conferma invio file\n");
		close(sk);
		return -1;
	}
	if (strcmp(mess,"SHARE FINISHED")==0){
		printf("Modalita' SHARE conclusa\n");
		return 0;
	}else{
		printf("ERRORE: impossibile ricevere messaggio conferma invio file\n");
		return -1;
	}
	/*
	*
	*MODICATO - FINE
	*	
	*/
}

//signal quit to server
int quit(int sk){
	printf("Chiusura connessione server...\n");
	if(send(sk, commands[3] , BUF_SIZE ,0) < BUF_SIZE){
		//error of sending message, quit anyway
		close(sk);
	}
	printf("Connessione chiusa!\n");
	close(sk);
	return 0;	
}

//Scanning keybord input in order to avoid overflow
//1 - input to scan
//2 - char scanned 
//3 - length to scan
int scanner(const char *input, char *buff_return, size_t buflen){
    char format[32];
    if (buflen == 0)
        return 0;
    snprintf(format, sizeof(format), "%%%ds", (int)(buflen-1));
    return sscanf(input, format, buff_return);
}