#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>  
#include "sha1.h"


#define CODA 3
#define MAX_CIFRE_UTENTI 3
#define VALORE_INIZIALE_SEMAFORO 1
#define NON_INIZIALIZZATO -1
#define INVIA_RICHIESTA 1
#define RICEVI_RICHIESTA 0
#define VALIDO 1
#define OK 1
#define NOK 0
#define DISPONIBILE 1
#define MSG_SIZE 1024
#define NICKNAME_SIZE 128
#define MAX_USERS 128
#define PIPE_ERROR -4
#define NOT_A_COMMAND 0
#define QUIT 1
#define LIST 2
#define HELP 3
#define EXIT 4
#define CANCEL 5
#define SHUTDOWN 6
#define PASSWORD "DED2D0BC 4F92FD91 58DDE711 CE2E8019 5FF5326E"
#define MAX_ATTEMPTS 3
#define TIMEOUT_EXPIRED -555
#define TIMEOUT_SERVER_SECS 60
#define TIMEOUT_SERVER_MICROSECS 0
#define USLEEP 500000

#ifdef DEBUG
#define LOG 1
#else
#define LOG 0
#endif

#define GENERIC_ERROR_HELPER(cond, errCode, msg) do {               \
  if (cond) {                                                       \
    fprintf(stderr, "%s: %s\n", msg, strerror(errCode));            \
    exit(EXIT_FAILURE);                                             \
  }                                                                 \
} while(0)

#define ERROR_HELPER(ret, msg)          GENERIC_ERROR_HELPER((ret < 0), errno, msg)
#define PTHREAD_ERROR_HELPER(ret, msg)  GENERIC_ERROR_HELPER((ret != 0), ret, msg)


/* DICHIARAZIONI*/
char* char2str(char *word);
int check_buff(char* buff, char ack);
char* senzaslashenne(char* nome);
int send_msg(int socket, char *msg);
size_t recv_msg(int socket, char *buff, size_t buf_len);
int message_action(char* buff) ;
int recv_and_parse(int socket, char* buff, size_t buff_len) ;
int send_and_parse(int socket, char* buff) ;
char* crypt(char* msg);
char* char2str(char *word);
int check_command(char* command, char* buff);
int check_quit(char* buff);
int check_cancel(char* buff);
int check_shutdown(char* buff);
int check_list(char* buff);
int check_help(char* buff);
int check_exit(char* buff);
int check_buff(char* buff, char ack) ;
/*-------------*/

typedef struct user_data_s {
  int     socket;
  char*    nickname;
  int*     valido;
  int     mode;
  int     disponibile;
} user_data_t;


typedef struct session_thread_args_s {
  int socket;
  struct sockaddr_in* address;
} session_thread_args_t;

typedef struct {
  int src;
  int dest;
} chat_args;


typedef struct{
  int socket;
  char* nickname;
}client_chat_arg;
