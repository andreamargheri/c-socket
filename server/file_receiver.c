#include "file_receiver.h"

const char* commands[] = {"!help","!list","!get","!quit"}; 
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
	
	//cast ad intero della porta
	porta = atoi(argv[2]);

	/* Creazione del socket : int socket(int domain, int type, int protocol); */
	sk = socket(PF_INET, SOCK_STREAM, 0);
	
	if (sk == -1){
		printf("Impossibile aprire il Socket\n");
		return -1;
	}
	
	//azzero la struttura dati server_addr
	memset(&server_addr,0,sizeof(server_addr));
	server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);
	server_addr.sin_port = htons(porta);
	
	if (inet_pton(AF_INET,argv[1],&server_addr.sin_addr) <= 0){
		//indirizzo non valido
		printf("Indirizzo %s:%d non valido!\n", argv[1],porta);	
		return -1;
	}

	ret = connect(sk,(struct sockaddr*) &server_addr,sizeof(server_addr));
	if (ret == -1){
		printf("Errore nella connessione con il server %s:%d\n",argv[1],porta);
		close(sk);
		return -1;
	}
	
	//ricevi il messaggio di benvenuto dal server
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
			//!list
			files_list(sk);
		}else if (strcmp(mess, commands[2])== 0){
			//!get
			files_get(sk);
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
	printf("*\t!list --> Visualizza la lista dei file scaricabili\n");
	printf("*\t!get --> Richiedi un file\n");
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


int files_list(int sk ){
	int i,n;
	char buff[BUF_SIZE];
	
	printf("Richiesta lista file server...\n");
	if (send(sk, commands[1] , BUF_SIZE ,0) < BUF_SIZE){
		printf("Errore nell'invio del comando al server\n");
		return -1;		
	}
	//#shared files
	if(recv(sk, buff, BUF_SIZE, MSG_WAITALL) < BUF_SIZE){
		printf("Errore nella ricezione lista file dal server\n");
		return -1;
	}
	n = atoi(buff);
	printf("Ci sono %d file disponibili:\n", n);
	
	for (i = 0; i < n ; i++){
		if(recv(sk, buff, BUF_SIZE, MSG_WAITALL) < BUF_SIZE){
			printf("Errore nella ricezione lista file dal server\n");
			return -1;
		}
		printf("%s\n",buff);
	}	
	return 0;
}

int files_get(int sk ){
	int ret,len;
	char input[BUF_SIZE],mess[BUF_SIZE],filename[BUF_SIZE];
	char *tmp;
	FILE* downloaded_file;
	char file_path[BUF_SIZE]="files_r/";
	
	//send get command
	if (send(sk, commands[2] , BUF_SIZE ,0) < BUF_SIZE){
		printf("Errore nell'invio del comando al server\n");
		return -1;		
	}
	
	//insert the file name to download
	printf("Inserisci il nome del file: ");
	scanf("%s",input);
	//scanning input
	scanner(input, filename, BUF_SIZE);
	//send file name to the server
	if (send(sk, filename, BUF_SIZE ,0)< BUF_SIZE){
		printf("Errore invio nome file al server.\n");
		return -1;
	}
	//wait for the server response
	ret = recv(sk, mess, BUF_SIZE, MSG_WAITALL); 
	if( (ret == -1) || (ret<BUF_SIZE) ) {
		printf("Errore connessione al server\n");
		close(sk);
		return -1;
	}
	//check if the response is ERR
	if (strcmp(mess,err_get[0])==0){
		//file exists but status no-shared
		printf("Impossibile scaricare il file %s: sender non disponibile\n",filename);
	}else if(strcmp(mess,err_get[1])==0){
		//file do not exits
		printf("Impossibile scaricare il file %s: file non presente nel sistema\n",filename);
		
	}else{
		//file available -> it has been received the sender name
		printf("File Sender %s disponibile al trasferimento\n",mess);
		//receive file dimension
		ret = recv(sk, mess, BUF_SIZE, MSG_WAITALL); 
		if( (ret == -1) || (ret<BUF_SIZE) ) {
			printf("Errore connessione al server\n");
			close(sk);
			return -1;
		}
		if (strcmp(mess,err_get[2])==0){
			printf("Errore del sender. Riprova\n");
			//close(sk);
			return -1;			
		}
		printf("Trasferimento del file %s in corso...\n",filename);
		//received the dimension in mess
		len = atoi(mess);
		tmp = (char*) malloc(sizeof(char) * len);
		//receive the file 
		if(recv(sk, tmp, sizeof(char) * len, MSG_WAITALL) < (sizeof(char) * len)){
			free(tmp);
			return -1;
		}
	
		strcat(file_path,filename);
		downloaded_file = fopen(file_path,"w");
		//try to open file
		if (downloaded_file == NULL){
			printf("Impossibile aprire il file %s\n",filename);
			free(tmp);
			return -1;
		}
		fwrite(tmp, sizeof(char), len, downloaded_file);
		fclose(downloaded_file);
	
		printf("Trasferimento del file %s terminato!\n",filename);
		
		free(tmp);
	}
	return 0;
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