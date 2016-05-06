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
sem_t users_sem,kill_sem;
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
	sem_wait_EH(users_sem,"chat_routine");
	print_utenti(users, MAX_USERS);
	sem_post_EH(users_sem,"chat_routine");
	int src=args->src;
	int dest=args->dest;
	int res;
	char msg[MSG_SIZE];
	while (1) {
		do {
			res=recv_msg(src, msg, MSG_SIZE);
		} while (res==0);
		if (res==-1) {	//il client ha chiuso la socket
			res=send_msg(dest,"#quit");
			if (res == PIPE_ERROR ) {
				close_connection(dest);
			}
			close_connection(src);
			sem_wait_EH(kill_sem, "chat_routine");
			kill_thread[src]=1;
			kill_thread[dest]=1;
			sem_post_EH(kill_sem, "chat_routine");
			pthread_exit(EXIT_SUCCESS);
		}
		res=send_msg(dest,msg);
		if (res==PIPE_ERROR){
			close_connection(dest);
			res=send_msg(src,"#quit");
			if (res == PIPE_ERROR ) {
				close_connection(src);
			}
			sem_wait_EH(kill_sem, "chat_routine");
			kill_thread[src]=1;
			kill_thread[dest]=1;
			sem_post_EH(kill_sem, "chat_routine");
			pthread_exit(EXIT_SUCCESS);
		}
		if (check_quit(msg) || check_exit(msg)) {
			printf("\nIl thread ha ricevuto %s. %sing chat...\n", msg, msg);
			if (check_exit(msg)) close_connection(src);
			sem_wait_EH(kill_sem, "chat_routine");
			kill_thread[src]=1;
			kill_thread[dest]=1;
			sem_post_EH(kill_sem, "chat_routine");
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
  printf("\nThread spawned.\n");
  int res, i, count, ret;
  memset(msg, 0, MSG_SIZE);
  res=recv_msg(socket, msg, MSG_SIZE); //riceve nickname
	nickname=char2str(msg);
  printf("\nIl nome del client è %s", nickname);
	int duplicato=OK;
	sem_wait_EH(users_sem,"thread_connection");
	for (i=0;i<MAX_USERS;i++) {
		if (*(users[i].valido)==VALIDO && strcmp(users[i].nickname, nickname)==0) {
			duplicato=NOK;
			printf("\nIl nome è già presente. Il thread e la connessione verranno chiusi.\n");
		}
	}
	sem_post_EH(users_sem,"thread_connection");
	if (socket>MAX_USERS || duplicato==NOK) {
    if (socket>MAX_USERS) printf("\nNon ci sono posti disponibili. Il thread e la connessione verranno chiusi.\n");
    res=send_msg(socket, "n");
    close(socket);
    pthread_exit(0);
	}
  else {
		sem_wait_EH(users_sem,"thread_connection");
		users[socket].socket=socket;
		users[socket].nickname=nickname;
		*(users[socket].valido)=VALIDO;
		users[socket].mode=NON_INIZIALIZZATO;
		users[socket].disponibile=DISPONIBILE;
	  res=send_msg(socket, "y");
		if (res == PIPE_ERROR ) {
			close_connection(socket);
		}
	  print_utenti(users, MAX_USERS);
		sem_post_EH(users_sem,"thread_connection");
	}

  while (1) {
		sleep(0.6f);
		//-------------invio lista-------------
		invia_lista(socket, msg);
		//-------------------------------------
    int mode;
    ricevi_modalita(socket, msg, nickname, &mode);


    /*---------------------/*
    /*  MODALITA ACCEPT   /*
    /*---------------------*/

    if (mode==1) {
      int indice_altroutente=routine_inoltra_richiesta(socket, msg, nickname);
			sem_wait_EH(users_sem,"thread_connection");
			chat_args arg_send={socket,users[indice_altroutente].socket};
			chat_args arg_rcv={users[indice_altroutente].socket, socket};
			sem_post_EH(users_sem,"thread_connection");
			pthread_t send,rcv;
			res=pthread_create(&send,NULL,chat_routine,(void*)&arg_send);
			PTHREAD_ERROR_HELPER(res,"\nimpossibile creare thread send");
			printf("\nCreata chat_routine con src: %d e dest: %d\n",socket, indice_altroutente);
			res=pthread_create(&rcv,NULL,chat_routine,(void*)&arg_rcv);
			PTHREAD_ERROR_HELPER(res,"\nimpossibile creare thread rcv");
			printf("\nCreata chat_routine con src: %d e dest: %d\n",indice_altroutente,socket);
			/*pthread_join(send,NULL);
			pthread_join(rcv,NULL);*/
			while (1) {
				sem_wait_EH(kill_sem, "thread_connection");
				if (kill_thread[socket] && kill_thread[indice_altroutente]) {
					pthread_cancel(send);
					pthread_cancel(rcv);
					break;
				}
				sem_post_EH(kill_sem, "thread_connection");
				sleep(1);
			}
			sem_wait_EH(kill_sem, "thread_connection");
			kill_thread[socket]=0;
			kill_thread[indice_altroutente]=0;
			sem_post_EH(kill_sem, "thread_connection");
			sem_wait_EH(users_sem,"thread_connection");
			users[indice_altroutente].mode=NON_INIZIALIZZATO;
			int check_valid=users[socket].valido;
			sem_post_EH(users_sem,"thread_connection");
			if(!check_valid){
				pthread_exit(EXIT_SUCCESS);
			}
    }

    /*---------------------/*
    /*  MODALITA RECEIVE   /*
    /*---------------------*/
		int flag;
    do {
			sem_wait_EH(users_sem,"thread_connection");
			if (*(users[socket].valido)==!VALIDO) {
				printf("\nIl thread di %s è stato terminato.\n", nickname);
				pthread_exit(0);
			}
			flag=users[socket].mode;
			sem_post_EH(users_sem,"thread_connection");
			sleep(0.5f);
    } while (flag==RICEVI_RICHIESTA);
		sem_wait_EH(users_sem,"thread_connection");
		users[socket].disponibile=DISPONIBILE;
		sem_post_EH(users_sem,"thread_connection");
  }


	sem_wait_EH(users_sem,"thread_connection");
  *(users[socket].valido)=!VALIDO;
	sem_post_EH(users_sem,"thread_connection");
	printf("\nIl thread di %s sta per essere terminato poichè non più valido.\n", nickname);
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
    ret = listen(server_desc, CODA);
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
    if(argc==2 && (strcmp(argv[1],"--help")==0 || strcmp(argv[1],"-h")==0)){
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

		ret=sem_init(&users_sem,0,VALORE_INIZIALE_SEMAFORO);
		ERROR_HELPER(ret,"cannot initializate semaphore users_sem ");
		ret=sem_init(&kill_sem,0,VALORE_INIZIALE_SEMAFORO);
		ERROR_HELPER(ret,"cannot initializate semaphore kill_sem ");

    // inizia ad accettare connessioni in ingresso sulla porta data
    listen_on_port(port_number_no);

    exit(EXIT_SUCCESS);
}

void do_message_action(int res, int socket, char* msg, char* nickname) {
  if (res==QUIT) {
		int ret;
		sem_wait_EH(users_sem,"do_message_action");
		int i;
    printf("\nMessage action: Il client %s ha deciso di voler uscire. \n", nickname);
		*(users[socket].valido)=!VALIDO;
		printf("\nIl thread di %s sta per essere terminato poichè non più valido.\n", nickname);
    print_utenti(users, MAX_USERS);
		sem_post_EH(users_sem,"do_message_action");
		close(socket);
    pthread_exit(0);
  }

  if (res==LIST) {
    invia_lista(socket, msg);
  }

	if (res==HELP) {
		//koffing
	}
	if (res==EXIT){

	}
}

void ricevi_modalita(int socket, char* msg, char* nickname, int* mode) {
  //recv_msg(socket, msg, 1);
  int res;
	do {
		res=recv_msg(socket, msg, MSG_SIZE);
	} while (res==0);
  if (check_quit(msg)) {
    close_connection(socket);
		pthread_exit(0);
  }
	int i;
  *mode=atoi(msg);
  printf("\nLa modalità di %s è %d\n", nickname, *mode);
  if (*mode==2) {
    close_connection(socket);
		pthread_exit(0);
  }
	sem_wait_EH(users_sem,"ricevi_modalita");
	users[socket].mode=(*mode);
	sem_post_EH(users_sem,"ricevi_modalita");
}

int routine_inoltra_richiesta(int socket, char* msg, char* nickname) {
	int indice_altro, i;
	sem_wait_EH(users_sem,"routine_inoltra_richiesta");
	users[socket].disponibile=!DISPONIBILE;
	print_utenti(users, MAX_USERS);
	sem_post_EH(users_sem,"routine_inoltra_richiesta");
	int trovato=0, res, indice_altroutente=-1;
  char* altronickname;
	user_data_t altroutente;
	altroutente.valido=(int*) malloc(sizeof(int));
  while (1) {
    printf("\nIn attesa che il client %s invii il nome con cui si vuole collegare\n", nickname);
    res=recv_and_parse(socket, msg, MSG_SIZE); //riceve nome altroutente
    if (res!=NOT_A_COMMAND) {
      do_message_action(res, socket, msg, nickname);
      continue;
    }
    altronickname=char2str(msg);
		printf("\n%s %s\n", nickname, altronickname);
    printf("\nIl client [%s] vuole collegarsi con [%s]", nickname, altronickname);
    if (strcmp(nickname, altronickname)==0) {
        printf("\nInviata stringa vuota o il nome di se stesso\n");
        res=send_msg(socket, "n");
				if (res == PIPE_ERROR ) {
					close_connection(socket);
				}
        continue;
    }
		sem_wait_EH(users_sem,"routine_inoltra_richiesta");
		for (i=0;i<MAX_USERS;i++) {
	    if (*(users[i].valido)==VALIDO && strlen(users[i].nickname)==strlen(altronickname) && strcmp(users[i].nickname, altronickname)==0) {
				indice_altroutente=i;
			}
	  }
		sem_post_EH(users_sem,"routine_inoltra_richiesta");

		printf("\nindice_altroutente: %d\n", indice_altroutente);

    if (indice_altroutente==-1) {
      printf("\nUtente non trovato.\n");
      res=send_msg(socket, "n");
			if (res == PIPE_ERROR ) {
				close_connection(socket);
			}
      continue;
    }
    else {
      printf("\nUtente trovato.\n");
    }

		res=send_msg(indice_altroutente, nickname);
		if (res == PIPE_ERROR ) {
			close_connection(socket);
		}
    printf("\nIn attesa di responso da parte di [%s]\n", altronickname);
    do {
      res=recv_msg(indice_altroutente, msg, MSG_SIZE);
    } while (res==0);
    printf("\nIl client %s ha risposto %c", altronickname, msg[0]);
    res=send_msg(socket, msg); //invia responso
		if (res == PIPE_ERROR ) {
			close_connection(socket);
		}
		sem_wait_EH(users_sem,"routine_inoltra_richiesta");
		int valid_flag=*(users[indice_altroutente].valido);
		sem_post_EH(users_sem,"routine_inoltra_richiesta");

		if (valid_flag==VALIDO && check_buff(msg, 'y')) {
			break;
		}
	}
	return indice_altroutente;
}

void invia_lista(int socket, char* msg) {
  int len=numero_disponibili(users, MAX_USERS), count=0;
  printf("\nLunghezza: %d\n", len );
	print_utenti(users,MAX_USERS);
  memset(msg, 0, MSG_SIZE);
  sprintf(msg, "%d", len);
  int res=send_msg(socket, msg); //invia lunghezza
	if (res == PIPE_ERROR ) {
		close_connection(socket);
	}
	int i;
  for (i=0;i<MAX_USERS;i++) {
		sem_wait_EH(users_sem,"invia_lista");
    if (*(users[i].valido)==VALIDO && users[i].disponibile==DISPONIBILE) {
			res=send_msg(socket, users[i].nickname);
			if (res == PIPE_ERROR ) {
				close_connection(socket);
			}
			printf("\nInviato %s\n", users[i].nickname);
		}
		sem_post_EH(users_sem,"invia_lista");
  }
}

void close_connection(int socket) {
  printf("\nIl client ha deciso di uscire. Shutting down thread...\n");
	sem_wait_EH(users_sem,"close_connection");
  printf("\nL'utente %s non è più valido\n", users[socket].nickname);
	*(users[socket].valido)=!VALIDO;
	if (LOG) printf("\nSe il numero [%d] corrisponde a 0 è stato eliminato correttamente.\n", *(users[socket].valido));
  print_utenti(users, MAX_USERS);
	sem_post_EH(users_sem,"close_connection");
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
	sem_wait_EH(users_sem,"numero_disponibili");
  for (i=0;i<dim;i++) {
    if (*(users[i].valido)==VALIDO && users[i].disponibile==DISPONIBILE) count++;
  }
	sem_post_EH(users_sem,"numero_disponibili");
  return count;
}

void sem_post_EH(sem_t sem, char* scope){
	int ret;
	ret=sem_post(&sem);
	char msg[MSG_SIZE];
	sprintf(msg,"cannot sem_post semaphore in %s\n",scope);
	ERROR_HELPER(ret, msg);
}

void sem_wait_EH(sem_t sem, char* scope){
	int ret;
	ret=sem_wait(&sem);
	char msg[MSG_SIZE];
	sprintf(msg,"cannot sem_wait semaphore in %s\n",scope);
	ERROR_HELPER(ret, msg);
}
