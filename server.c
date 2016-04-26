#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include "util.c"

user_data_t users[MAX_USERS];
sem_t sem;
int kill_thread[MAX_USERS]={0};

/* DICHIARAZIONI FUNZIONI */
void do_message_action(int res, int socket, char* buff, char* nickname);
void ricevi_modalita(int socket, char* msg, char* nickname, int* mode);
int routine_inoltra_richiesta(int socket, char* msg, char* nickname);
void invia_lista(int socket, char* msg);
void close_connection(int socket);
void print_utenti(user_data_t users[], int dim);
void print_utenti(user_data_t users[], int dim);
int numero_disponibili(user_data_t users[], int dim);
/*-----------------------*/

void gestione_interrupt() {
	printf("\n[SERVER]\tTerminazione del server....\n");
	exit(0);
}

void* chat_routine(void* arg) {
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
	chat_args* args=(chat_args*) arg;
	if (LOG) printf("\n Creato thread chat_routine\n");
	print_utenti(users, MAX_USERS);
	int src=args->src;
	int dest=args->dest;
	int res;
	char msg[MSG_SIZE];
	while (1) {
		do {
			res=recv_msg(src, msg, MSG_SIZE);
		} while (res==0);
		if (res==-1) {
			printf("\nLa connessione verrà chiusa...\n");
			pthread_exit(0);
		}
		if(LOG) printf("\nthread con src %s e dest %s[%d] ricevuto: %s \n",users[src].nickname,users[dest].nickname,dest,msg);
		if (*(users[dest].valido)==VALIDO) send_msg(dest,msg);
		if(LOG) printf("\nthread con src %s e dest %s[%d] inviato: %s \n",users[src].nickname,users[dest].nickname,dest,msg);
		if (check_quit(msg)) {
			if (LOG) printf("\nIl thread ha ricevuto #quit. Exit...\n");
			close_connection(src);
			kill_thread[src]=1;
			kill_thread[dest]=1;
			pthread_exit(EXIT_SUCCESS);
		}
	}
}


void* thread_connection(void* arg) {
  session_thread_args_t* args = (session_thread_args_t*)arg;
  int socket=args->socket;
	int indice_utente;
  char msg[MSG_SIZE];
  char* nickname;
  if (LOG) printf("\nThread spawned. Socket_ds: %d\n", socket);
  int res, i, count;
  memset(msg, 0, MSG_SIZE);
  res=recv_msg(socket, msg, MSG_SIZE); //riceve nickname
	nickname=char2str(msg);
  if (LOG) printf("\nIl nome del client è %s", nickname);
	int duplicato=OK;
	for (i=0;i<MAX_USERS;i++) {
		if (*(users[i].valido)==VALIDO && strcmp(users[i].nickname, nickname)==0) {
			duplicato=NOK;
			printf("\nIl nome è già presente. Il thread e la connessione verranno chiusi.\nValido?[%d]\"%s\"==\"%s\"",*(users[i].valido),users[i].nickname,nickname );
		}
	}
	if (socket>MAX_USERS) {
    printf("\nNon ci sono posti disponibili. Il thread e la connessione verranno chiusi.\n");
    send_msg(socket, "n");
    close(socket);
    pthread_exit(0);
	}
  if (duplicato==NOK) {
    send_msg(socket, "n");
    close(socket);
    pthread_exit(0);
  }
  else {
		users[socket].socket=socket;
		users[socket].nickname=nickname;
		*(users[socket].valido)=VALIDO;
		users[socket].mode=NON_INIZIALIZZATO;
		users[socket].disponibile=DISPONIBILE;
		if (LOG) printf("\nusers[socket].nickname: %s\n", users[socket].nickname);
		if (LOG) printf("\nusers[socket].socket: %d\n", users[socket].socket);
	  send_msg(socket, "y");
	  print_utenti(users, MAX_USERS);
	}

  while (1) {
		//-------------invio lista-------------
		if (LOG) printf("\nInizio a spedire la lista\n");
		invia_lista(socket, msg);
		//-------------------------------------
    int mode;
		if(LOG) printf("\nInizio a ricevere modalita\n" );
    ricevi_modalita(socket, msg, nickname, &mode);


    /*---------------------/*
    /*  MODALITA ACCEPT   /*
    /*---------------------*/

    if (mode==1) {
      int indice_altroutente=routine_inoltra_richiesta(socket, msg, nickname);
			chat_args arg_send={socket,users[indice_altroutente].socket};
			chat_args arg_rcv={users[indice_altroutente].socket, socket};
			pthread_t send,rcv;
			res=pthread_create(&send,NULL,chat_routine,(void*)&arg_send);
			PTHREAD_ERROR_HELPER(res,"\nimpossibile creare thread send");
			if (LOG) printf("\nCreata chat_routine con src: %d e dest: %d\n",socket, users[indice_altroutente].socket);
			res=pthread_create(&rcv,NULL,chat_routine,(void*)&arg_rcv);
			PTHREAD_ERROR_HELPER(res,"\nimpossibile creare thread rcv");
			if (LOG) printf("\nCreata chat_routine con src: %d e dest: %d\n",users[indice_altroutente].socket,socket);
			/*pthread_join(send,NULL);
			pthread_join(rcv,NULL);*/
			while (1) {
				if (kill_thread[socket] || kill_thread[users[indice_altroutente].socket]) {
					if (LOG) printf("\nKILL_THREADDALI!!!!!\n");
					pthread_cancel(send);
					pthread_cancel(rcv);
					break;
				}
				sleep(1);
			}
			if (LOG) printf("\nJoinati i thread in mode=1\n");
			users[indice_altroutente].mode=NON_INIZIALIZZATO;
			kill_thread[socket]=0;
			kill_thread[indice_altroutente]=0;
    }

    /*---------------------/*
    /*  MODALITA RECEIVE   /*
    /*---------------------*/
    do {
			if (*(users[socket].valido)==!VALIDO) {
				if (LOG) printf("\nIl thread è stato terminato.\n");
				pthread_exit(0);
			}
			sleep(1);
    } while (users[socket].mode==RICEVI_RICHIESTA);  //TODO da modificare

		users[socket].disponibile=DISPONIBILE;

  }


  if (LOG) printf("\nShutting down thread...\n");
  *(users[socket].valido)=!VALIDO;
	if (LOG) printf("\nIl thread di %s sta per essere terminato poichè non più valido.\n\nSe il numero [%d] è 0 allora è stato eliminato correttamente.\n", nickname, *(users[socket].valido));
  print_utenti(users, MAX_USERS);
  close(socket);
  pthread_exit(0);
}







void listen_on_port(unsigned short port_number_no) {
    int ret;
    int server_desc, client_desc;

    struct sockaddr_in server_addr = {0};
    int sockaddr_len = sizeof(struct sockaddr_in); // usato da accept()

    // impostazioni per le connessioni in ingresso
    server_desc = socket(AF_INET , SOCK_STREAM , 0);
    ERROR_HELPER(server_desc, "Impossibile creare socket server_desc");

    server_addr.sin_addr.s_addr = INADDR_ANY; // accettiamo connessioni da tutte le interfacce (es. lo 127.0.0.1)
    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = port_number_no;

    int reuseaddr_opt = 1; // SO_REUSEADDR permette un riavvio rapido del server dopo un crash
    ret = setsockopt(server_desc, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_opt, sizeof(reuseaddr_opt));
    ERROR_HELPER(ret, "Impossibile settare l'opzione SO_REUSEADDR");

    // binding dell'indirizzo alla socket
    ret = bind(server_desc, (struct sockaddr*) &server_addr, sockaddr_len);
    ERROR_HELPER(ret, "Impossibile eseguire bind su socket_desc");

    // marca la socket come passiva per mettersi in ascolto
    ret = listen(server_desc, MAX_CONN_QUEUE);
    ERROR_HELPER(ret, "Impossibile eseguire listen su socket_desc");

    struct sockaddr_in* client_addr = calloc(1, sizeof(struct sockaddr_in));


    // accetta connessioni in ingresso
    while (1) {
        /** client_addr è il bufffer da usare per la connessione in ingresso che sto per accettare **/

        client_desc = accept(server_desc, (struct sockaddr*)client_addr, (socklen_t*) &sockaddr_len);
        if (client_desc == -1 && errno == EINTR) continue;
        ERROR_HELPER(client_desc, "Impossibile eseguire accept su socket_desc");

        session_thread_args_t* args = (session_thread_args_t*)malloc(sizeof(session_thread_args_t));
        args->socket = client_desc;
        args->address = client_addr;

        // inizia una sessione di chat per l'utente in ingresso
        pthread_t thread;
        ret = pthread_create(&thread, NULL, thread_connection, args);
        PTHREAD_ERROR_HELPER(ret, "Errore nella creazione di un thread");

        ret = pthread_detach(thread);
        PTHREAD_ERROR_HELPER(ret, "Errore nel detach di un thread");

        // alloco un nuovo bufffer per servire la prossima connessione in ingresso
        client_addr = calloc(1, sizeof(struct sockaddr_in));
    }
}




int main(int argc, char* argv[]) {
    if(argc==2 && strcmp(argv[1],"--help")==0){
      printf("Lato server dell'applicazione talk, Usage ./server -p <port>\n");
      exit(1);
    }
    if (argc!=3 || strcmp(argv[1],"-p")!=0) {
      printf("\nUsage: ./server -p <port>\n");
      exit(1);
    }//parse command line
		int i;
		for (i=0;i<MAX_USERS;i++) { //inizializzazione
			users[i].socket=-1;
			users[i].nickname="";
			users[i].valido=(int*) malloc(sizeof(int));
			*(users[i].valido)=!VALIDO;
			users[i].mode=NON_INIZIALIZZATO;
			users[i].disponibile=!DISPONIBILE;
	  }

    printf("\nServer lanciato sulla porta %s\n", argv[2]);
    int ret;

    signal(SIGINT, gestione_interrupt);

    unsigned short port_number_no; // il suffisso "_no" sta per network byte order

    // ottieni il numero di porta da usare per il server dall'argomento del comando
    long tmp = strtol(argv[2], NULL, 0);
    if (tmp < 1024 || tmp > 49151) {
        fprintf(stderr, "Errore: utilizzare un numero di porta compreso tra 1024 e 49151.\n");
        exit(EXIT_FAILURE);
    }
    port_number_no = htons((unsigned short)tmp);  //host to network short

    // inizia ad accettare connessioni in ingresso sulla porta data
    listen_on_port(port_number_no);

    exit(EXIT_SUCCESS);
}


/*
#define QUIT 1
#define LIST 2
#define HELP 3
*/

void do_message_action(int res, int socket, char* msg, char* nickname) {
  if (res==QUIT) {
		int i;
    printf("\nMessage action: Il client %s ha deciso di voler uscire. \n", nickname);
		*(users[socket].valido)=!VALIDO;
		if (LOG) printf("\nIl thread di %s sta per essere terminato poichè non più valido.\n", nickname);
		if (LOG) printf("\nSe il numero [%d] è 0 allora è stato eliminato correttamente.\n", *(users[socket].valido));
    print_utenti(users, MAX_USERS);
    close(socket);
    pthread_exit(0);
  }

  if (res==LIST) {
    invia_lista(socket, msg);
  }

	if (res==HELP) {
		//nothing
	}
}




void ricevi_modalita(int socket, char* msg, char* nickname, int* mode) {
  if (LOG) printf("\nRicezione modalità da parte di %s \n",nickname );
  //recv_msg(socket, msg, 1);
  int res=recv_msg(socket, msg, MSG_SIZE);
  if (check_quit(msg)) {
    close_connection(socket);
		pthread_exit(0);
  }
	int i;
  *mode=atoi(msg);
  if (LOG) printf("\nLa modalità di %s è %d\n", nickname, *mode);
  if (*mode==2) {
    close_connection(socket);
		pthread_exit(0);
  }
	users[socket].mode=(*mode);
	if (LOG) printf("\nLa modalità di %s è stata cambiata: %d\n", nickname, users[socket].mode);
}



int routine_inoltra_richiesta(int socket, char* msg, char* nickname) {
	int indice_altro, i;
	users[socket].disponibile=!DISPONIBILE;
	if (LOG) printf("\nLa disponibilità di %s è stata cambiata: %d\t Corrisponde a 0?", nickname, users[socket].disponibile);
	print_utenti(users, MAX_USERS);
  int trovato=0, res, indice_altroutente=-1;
  char* altronickname;
	user_data_t altroutente;
	altroutente.valido=(int*) malloc(sizeof(int));
  while (1) {
    if (LOG) printf("\nIn attesa che il client %s invii il nome con cui si vuole collegare\n", nickname);
    res=recv_and_parse(socket, msg, MSG_SIZE); //riceve nome altroutente
    if (res!=NOT_A_COMMAND) {
      do_message_action(res, socket, msg, nickname);
      continue;
    }
    altronickname=char2str(msg);
		printf("\n%s %s\n", nickname, altronickname);
    if (LOG) printf("\nIl client [%s] vuole collegarsi con [%s]", nickname, altronickname);
    if (strcmp(nickname, altronickname)==0) {
        printf("\nInviata stringa vuota o il nome di se stesso\n");
        send_msg(socket, "n");
        continue;
    }

		for (i=0;i<MAX_USERS;i++) {
	    if (*(users[i].valido)==VALIDO && strlen(users[i].nickname)==strlen(altronickname) && strcmp(users[i].nickname, altronickname)==0) {
				indice_altroutente=i;
			}
	  }

		printf("\nindice_altroutente: %d\n", indice_altroutente);

    if (indice_altroutente==-1) {
      printf("\nUtente non trovato.\n");
      send_msg(socket, "n");
      continue;
    }
    else {
      printf("\nUtente trovato.\n");
      //send_msg(socket, "y");
    }
    int altrosocket=users[indice_altroutente].socket;
    send_msg(altrosocket, nickname);
    if (LOG) printf("\nIn attesa di responso da parte di [%s]\n", altronickname);
    do {
      res=recv_msg(altrosocket, msg, MSG_SIZE);
    } while (res==0);
    if (LOG) printf("\nIl client %s ha risposto %c", altronickname, msg[0]);
    send_msg(socket, msg); //invia responso
		if (*(users[indice_altroutente].valido)==VALIDO && check_buff(msg, 'y')) break;
	}
	return indice_altroutente;
}

void invia_lista(int socket, char* msg) {
  int len=numero_disponibili(users, MAX_USERS), count=0;
  if (LOG) printf("\nLunghezza: %d\n", len );
  memset(msg, 0, MSG_SIZE);
  sprintf(msg, "%d", len);
  send_msg(socket, msg); //invia lunghezza
	int i;
  for (i=0;i<MAX_USERS;i++) {
    if (*(users[i].valido)==VALIDO && users[i].disponibile==DISPONIBILE) {
			send_msg(socket, users[i].nickname);
			if (LOG) printf("\nInviato %s\n", users[i].nickname);
		}
  }
}

void close_connection(int socket) {
  printf("\nIl client ha deciso di uscire. Shutting down thread...\n");
  if (LOG) printf("\nL'utente %s non è più valido\n", users[socket].nickname);
	*(users[socket].valido)=!VALIDO;
	if (LOG) printf("\nSe il numero [%d] corrisponde a 0 è stato eliminato correttamente.\n", *(users[socket].valido));
  //print_utenti(users, MAX_USERS);
	print_utenti(users, MAX_USERS);
	close(socket);
}

void print_utenti(user_data_t users[], int dim) {
  int count=0, i;
  printf("\n------------------UTENTI ON LINE------------------\n");
  for (i=0;i<dim;i++) {
    if (*(users[i].valido)==VALIDO) {
      count++;
      printf("\n%d - %s\n", count, users[i].nickname);
    }
  }
  if (count==0) printf("\nNessun Utente Online\n");
  printf("\n--------------------------------------------------\n");
}

int numero_disponibili(user_data_t users[], int dim) {
  int count=0, i;
  for (i=0;i<dim;i++) {
    if (*(users[i].valido)==VALIDO && users[i].disponibile==DISPONIBILE) count++;
  }
  return count;
}

/*
void send_SOS(int sock_dest){
    int i=0,num_help_i=4;
    char* num_help;
		sprintf(num_help, "%d", num_help_i);
    char* welcome="\n\nHi welcome to talk application, type #command:";
    char* quit="\n---- #quit to leave your awesome application";
    char* help="\n---- #help to ask an SOS";
    char* list="\n---- #list to refresh user list\n";
    char* SOS[]={welcome,quit,help,list};
    send_msg(sock_dest,num_help);
    while(i<num_help_i){
      send_msg(sock_dest,SOS[i]);
      if(LOG) printf("\ninviato %s ;",SOS[i]);
      i++;
    }
}*/
