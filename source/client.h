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
#include <termios.h>


sem_t kill_sem;
int sock, kill_thread;

/* DICHIARAZIONI FUNZIONI */
void ricevi_lista(int sock, char* buff);
void sigquit();
void sigint();
void sigterm();
void sighup();
void sigsegfault();
static void gestione_interrupt(int signo);
void* recv_routine(void* arg);
void* send_routine(void* arg);
void do_message_action(int res, int socket, char* msg,char* nickname);
void ricevi_lista(int sock, char* buff) ;
void sem_post_EH(sem_t sem, char* scope);
void sem_wait_EH(sem_t sem, char* scope);
void gestione_admin(int socket);

/*-----------------------*/
