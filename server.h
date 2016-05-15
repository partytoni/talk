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
#include <poll.h>


user_data_t users[MAX_USERS];
sem_t users_sem,kill_sem, receive_sem;
int kill_thread[MAX_USERS]={0};
int receive_flag[MAX_USERS]={0};
pthread_t cancel_from_admin[MAX_USERS];

/* DICHIARAZIONI FUNZIONI */
void sigquit();
void sigint();
void sigsegfault();
void sigterm();
void sighup();
static void gestione_interrupt(int signo);
void * chat_routine(void * arg);
void * thread_connection(void * arg);
void listen_on_port(unsigned short port_number_no);
void do_message_action(int res, int socket, char* buff, char* nickname);
void ricevi_modalita(int socket, char* msg, char* nickname, int* mode);
int routine_inoltra_richiesta(int socket, char* msg, char* nickname);
void invia_lista(int socket, char* msg);
void close_connection(int socket);
void print_utenti(user_data_t users[], int dim);
int numero_disponibili(user_data_t users[], int dim);
void sem_post_EH(sem_t sem,char * scope);
void sem_wait_EH(sem_t sem,char * scope);
int lunghezza(user_data_t users[], int dim);
void invia_lista_admin(int socket,char* msg);
void gestione_admin(int socket);
/*-----------------------*/
