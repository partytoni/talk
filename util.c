#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>

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
#define PIPE_ERROR -1
#define NOT_A_COMMAND 0
#define QUIT 1
#define LIST 2
#define HELP 3
#define EXIT 4
#define CANCEL 5
#define SHUTDOWN 6
#define PASSWORD "lucascemo"
#define MAX_ATTEMPTS 3

#ifdef DEBUG
  #define LOG 1
#else
  #define LOG 0
#endif

#define GENERIC_ERROR_HELPER(cond, errCode, msg) do {               \
        if (cond) {                                                 \
            fprintf(stderr, "%s: %s\n", msg, strerror(errCode));    \
            exit(EXIT_FAILURE);                                     \
        }                                                           \
    } while(0)

#define ERROR_HELPER(ret, msg)          GENERIC_ERROR_HELPER((ret < 0), errno, msg)
#define PTHREAD_ERROR_HELPER(ret, msg)  GENERIC_ERROR_HELPER((ret != 0), ret, msg)


/* DICHIARAZIONI*/
char* char2str(char *word);
int check_buff(char* buff, char ack);
/*-------------*/

int send_msg(int socket, const char *msg) {
    int ret;
    char msg_to_send[MSG_SIZE];
    sprintf(msg_to_send, "%s\n", msg);
    int bytes_left = strlen(msg_to_send);
    int bytes_sent = 0;
    while (bytes_left > 0) {
        ret = send(socket, msg_to_send + bytes_sent, bytes_left, MSG_NOSIGNAL);
        if (ret == -1 && errno == EINTR) continue;
        if (ret == -1 && errno == EPIPE) return PIPE_ERROR;
        ERROR_HELPER(ret, "Errore nella scrittura su socket");

        bytes_left -= ret;
        bytes_sent += ret;
    }
    return bytes_sent;
}

/*
 * Riceve un messaggio dalla socket desiderata e lo memorizza nel
 * buffer buff di dimensione massima buf_len bytes.
 *
 * La fine di un messaggio in entrata è contrassegnata dal carattere
 * speciale '\n'. Il valore restituito dal metodo è il numero di byte
 * letti ('\n' escluso), o -1 nel caso in cui il client ha chiuso la
 * connessione in modo inaspettato.
 */
size_t recv_msg(int socket, char *buff, size_t buf_len) {
    int ret;
    int bytes_read = 0;
    memset(buff, 0, buf_len);
    // messaggi più lunghi di buf_len bytes vengono troncati
    while (bytes_read <= buf_len) {
        ret = recv(socket, buff + bytes_read, 1, 0);

        if (ret == 0) return -1; // il client ha chiuso la socket
        if (ret == -1 && errno == EINTR) continue;
        if (ret == -1 && errno != EINTR)  {
          printf("\nErrore nella lettura da socket\n");
          return -1;
        }
        // controllo ultimo byte letto
        if (buff[bytes_read] == '\n') break; // fine del messaggio: non incrementare bytes_read

        bytes_read++;
    }

    /* Quando un messaggio viene ricevuto correttamente, il carattere
     * finale '\n' viene sostituito con un terminatore di stringa. */
    buff[bytes_read] = '\0';
    return bytes_read; // si noti che ora bytes_read == strlen(buff)
}

int message_action(char* buff) {
  if (check_quit(buff)) return QUIT;
  if (check_list(buff)) return LIST;
  if (check_help(buff)) return HELP;
  if (check_exit(buff)) return EXIT;
  if (check_cancel(buff)) return CANCEL;
  if (check_shutdown(buff)) return SHUTDOWN;
  else return NOT_A_COMMAND;
}

int recv_and_parse(int socket, char* buff, size_t buff_len) {
  int res;
  do {
    res=recv_msg(socket, buff, buff_len);
  } while (res==0);
  return message_action(buff);
}

int send_and_parse(int socket, char* buff) {
  int res;
  send_msg(socket, buff);
  if (LOG) printf("\nSEND_AND_PARSE: %d", message_action(buff));
  return message_action(buff);

}
/*
*******************************************************************
*/

typedef struct user_data_s {
    int     socket;
    char*    nickname;
    int*     valido;
    int     mode;
    int     disponibile;
    int socket_altroutente;
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


char* senzaslashenne(char* nome) {
  int count=0, len=strlen(nome);
  char* ris=(char*) malloc(sizeof(char)*strlen(nome));
  while (count<len) {
    if (nome[count]=='\n') ris[count]='\0';
    else ris[count]=nome[count];
    count++;
  }
  return ris;
}

char* char2str(char *word){
  char *w = malloc((strlen(word)+1)* sizeof(char));
  strcpy(w, word);
  return w;
}

int check_command(char* command, char* buff) {
  char* buffstr=senzaslashenne(char2str(buff));
  if (strlen(buffstr)==strlen(command) && strcmp(buffstr, command)==0) return 1;
  else return 0;
}

int check_quit(char* buff) {
  return check_command("#quit", buff);
}

int check_cancel(char* buff) {
  return check_command("#cancel", buff);
}

int check_shutdown(char* buff) {
  return check_command("#shutdown", buff);
}

int check_list(char* buff) {
  return check_command("#list", buff);
}

int check_help(char* buff) {
  return check_command("#help", buff);
}

int check_exit(char* buff) {
  return check_command("#exit", buff);
}

int check_buff(char* buff, char ack) {
  if (buff[0]==ack) return 1;
  else return 0;
}
