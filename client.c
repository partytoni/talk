#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <semaphore.h>
#include <signal.h>
#include "util.c"

/* DICHIARAZIONI FUNZIONI */
void ricevi_lista(int sock, char* buff);
void do_message_action(int res, int socket, char* msg);
/*-----------------------*/
sem_t kill_sem;
int sock, kill_thread;


void gestione_interrupt() {
	printf("\nTerminazione richiesta. Chiusura delle connessioni attive...\n");
	send_msg(sock, "#exit");
	close(sock);
	exit(0);
}

void* recv_routine(void* arg){
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
	client_chat_arg* args=(client_chat_arg*)arg;
	int socket=args->socket, res;
	char* nickname=args->nickname;
	char buff[MSG_SIZE];
	while(1){
		res=recv_msg(socket,buff,MSG_SIZE);
		if (res==0) continue;
		if (res==-1) {
			printf("\nIl client %s ha terminato la connessione. EXITING...\n", nickname);
			pthread_exit(0);
		}
		if (check_quit(buff) || check_exit(buff) ) {
			printf("\nIl client [%s] è uscito dalla chat...\n", nickname);
			sem_wait_EH(kill_sem,"recv_routine");
			kill_thread=1;
			sem_post_EH(kill_sem,"recv_routine");
			pthread_exit(0);
		}

		printf("\n[%s]\t%s\n",nickname,buff );
	}
}

void* send_routine(void* arg){
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
	client_chat_arg* args=(client_chat_arg*)arg;
	int socket=args->socket;
	char* nickname=args->nickname;
	char buff[MSG_SIZE];
	while(1){
		memset(buff,0,MSG_SIZE);
		fgets(buff,MSG_SIZE,stdin);
		if(strlen(buff)==1) continue;
		send_msg(socket,buff);
		if (check_quit(buff)) {
			printf("\nHai inviato #quit. pthread_exit\n");
			sem_wait_EH(kill_sem,"send_routine");
			kill_thread=1;
			sem_post_EH(kill_sem,"send_routine");
			pthread_exit(0);
		}
		printf("\n[%s]\t%s\n",nickname,buff );
	}
}


int main(int argc, char* argv[]) {

  if(argc==2 && strcmp(argv[1],"--help")==0){
		printf("lato client dell'applicazione talk, per connettersi al server e parlare con altri utenti usare : ./client -a <ServerAddress> -p <port> -u <USERNAME>\n");
		exit(1);
	}

	if (argc!=7 || strcmp(argv[1],"-a")!=0 || strcmp(argv[3],"-p")!=0 || strcmp(argv[5],"-u")!=0) {
		printf("\nUsage: ./client -a <address> -p <port> -u <user>\n");
		exit(1);
	}//parse command line

  if (LOG) printf("\nUtente %s, porta %s\n", argv[6], argv[4]);

  signal(SIGINT, gestione_interrupt);
	signal(SIGQUIT,gestione_interrupt);
  // we use network byte order
  in_addr_t ip_addr;
  unsigned short port_number_no;
  char nickname[MSG_SIZE];

  // resrieve IP address
  ip_addr = inet_addr(argv[2]); // we omit error checking

  // resrieve port number
  long tmp = strtol(argv[4], NULL, 0); // safer than atoi()
  if (tmp < 1024 || tmp > 49151) {
      fprintf(stderr, "Please use a port number between 1024 and 49151.\n");
      exit(EXIT_FAILURE);
  }
  port_number_no = htons((unsigned short)tmp);

  sprintf(nickname, "%s", argv[6]);

  int res;
  struct sockaddr_in server_addr = {0}; // some fields are required to be filled with 0

  // create sock
  sock = socket(AF_INET, SOCK_STREAM, 0);
  ERROR_HELPER(sock, "Could not create sock");

  // set up parameters for the connection
  server_addr.sin_addr.s_addr = ip_addr;
  server_addr.sin_family      = AF_INET;
  server_addr.sin_port        = port_number_no;

  // initiate a connection on the sock
  res = connect(sock, (struct sockaddr*) &server_addr, sizeof(struct sockaddr_in));
  ERROR_HELPER(res, "Could not create connection");
  if (LOG) printf("\nConnesso al server.\n");

  char buff[MSG_SIZE];
  send_msg(sock, argv[6]); //invia nome
  recv_msg(sock, buff, 1);
  if (check_buff(buff, 'y')) printf("\nIl nome è disponibile\n");
  else {
    printf("\nIl nome non è disponibile. EXITING...\n");
    exit(1);
  }

	res=sem_init(&kill_sem,0,VALORE_INIZIALE_SEMAFORO);
	ERROR_HELPER(res,"cannot initializate semaphore");

	while (1) {
		if (LOG) printf("\n Inizio a ricevere la lista.\n");
	  ricevi_lista(sock, buff);

	  while (1) {
	    printf("\n Premi 0 per ricevere una richiesta, 1 per inoltrarla, 2 per uscire.\n");
	    memset(buff, 0, MSG_SIZE);
	    fgets(buff, MSG_SIZE, stdin);
	    if (strlen(buff)==2 && (check_buff(buff, '0') || check_buff(buff, '1') || check_buff(buff, '2'))) break;
	  }
	  send_msg(sock, buff); //invia modalita
	  if (check_buff(buff, '2')) {
	    printf("\n Hai deciso di uscire. EXITING...\n");
	    exit(0);
	  }

	  int mode;
	  if (check_buff(buff, '0')) mode=0;
	  if (check_buff(buff, '1')) mode=1;

		char* altronickname;

	  if (mode==1) { //mode==1
	    int res;
	    while (1) {
	      printf("\nCon chi vuoi collegarti? #exit per uscire\n");
	      memset(buff, 0, MSG_SIZE);
	      fgets(buff, MSG_SIZE, stdin);
	      if (strlen(buff)==1) sprintf(buff, "%s", nickname);
	      res=send_and_parse(sock, buff);
	      if (res!=NOT_A_COMMAND) {
	        do_message_action(res, sock, buff);
	        continue;
	      }
	      altronickname=senzaslashenne(char2str(buff));
				printf("\nIn attesa di risposta da parte di %s\n", altronickname);
				recv_msg(sock, buff, MSG_SIZE);
	      if (check_buff(buff,'y')) {
	        printf("\nL'utente [%s] ha accettato la richiesta!\n", altronickname);
	        break;
	      }
	      if (check_buff(buff,'n')) printf("\nUtente [%s] non trovato o non disponibile per la chat!\n", altronickname);
	    }
			pthread_t send,rcv;
			client_chat_arg arg1={sock,altronickname};
			client_chat_arg arg2={sock,argv[6]};
			printf("\nLa connessione con %s è stata abilitata.\n", altronickname);
			res=pthread_create(&send,NULL,send_routine,(void*)&arg2);
			PTHREAD_ERROR_HELPER(res,"spawning new send_routine thread!");
			res=pthread_create(&rcv,NULL,recv_routine,(void*)&arg1);
			PTHREAD_ERROR_HELPER(res,"spawning new recv_routine thread!");
			while (1) {
				sem_wait_EH(kill_sem,"main");
				if (kill_thread) {
					pthread_cancel(send);
					pthread_cancel(rcv);
					break;
				}
				sem_post_EH(kill_sem,"main");
				sleep(0.5f);
			}
	  }


	  int accepted=0;
	  while (mode==0 && accepted==0) {
	    printf("\nSei in attesa di richiesta di chat\n");
	    do{
				res=recv_msg(sock, buff, MSG_SIZE); //legge nome che vuole collegarsi con lui
			}while(res==0);
			altronickname=char2str(buff);
	    printf("\nVuoi collegarti con %s?\n", altronickname);
	    do{
	      printf("\nInserisci y oppure n.\n");
	      memset(buff, 0, MSG_SIZE);
	      fgets(buff, MSG_SIZE, stdin);
	      if (check_buff(buff, 'y')) accepted=1;
	    } while(strlen(buff)==1 || (!check_buff(buff, 'y') && !check_buff(buff, 'n')));
	    if (LOG) printf("\nInvio risposta: %c\n", buff[0]);
	    send_msg(sock, buff);
			if (check_buff(buff, 'n')) continue;
			pthread_t send,rcv;
			client_chat_arg arg1={sock,altronickname};
			client_chat_arg arg2={sock,argv[6]};
			res=pthread_create(&send,NULL,send_routine,(void*)&arg2);
			PTHREAD_ERROR_HELPER(res,"spawning new send_routine thread!");
			res=pthread_create(&rcv,NULL,recv_routine,(void*)&arg1);
			PTHREAD_ERROR_HELPER(res,"spawning new recv_routine thread!");

			while (1) {
				sem_wait_EH(kill_sem,"main");
				if (kill_thread) {
					pthread_cancel(send);
					pthread_cancel(rcv);
					break;
				}
				sem_post_EH(kill_sem,"main");
				sleep(0.5f);
			}
	  }
		sem_wait_EH(kill_sem,"main");
		kill_thread=0;
		sem_post_EH(kill_sem,"main");
	}
  close(sock);
  exit(EXIT_SUCCESS);
}

void do_message_action(int res, int socket, char* msg) {
  if (res==LIST) {
    ricevi_lista(socket, msg);
  }

	if (res==HELP) {
		char* welcome="\n\nHi welcome to talk application, type #command:";
    char* quit="\n---- #quit to leave the current chat (if opened)";
    char* help="\n---- #help to ask an SOS";
    char* list="\n---- #list to refresh user list";
		char* _exit="\n---- #exit to leave your awesome application\n";
    char* SOS[]={welcome,quit,help,list, _exit};
		int i=0;
    while(i<5){
      if(LOG) printf("%s",SOS[i]);
      i++;
    }
	}
	if (res==EXIT){
		printf("\nHai deciso di voler uscire. \n");
    close(socket);
    exit(0);
	}
}

void ricevi_lista(int sock, char* buff) {
	int res;
  do {
		res=recv_msg(sock, buff, MSG_SIZE);
	} while (res==0);
  int len=atoi(buff), count=0;
  if (LOG) printf("\nLunghezza: %d\n", len);
  printf("\n------------------UTENTI ONLINE--------------\n");
  while (count<len) {
    count++;
    memset(buff, 0, MSG_SIZE);
		do {
			res=recv_msg(sock, buff, MSG_SIZE);
		} while (res==0);
    printf("\n%d - %s\n", count, buff);
  }
  if (len==0) printf("\nNessun utente online disponibile\n");
  printf("\n---------------------------------------------\n");
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
