
#include "util.h"


int send_msg(int socket, char *msg) {
  int ret;
  while (1) {
    ret=send(socket, msg, MSG_SIZE, MSG_NOSIGNAL);
    if (ret==-1 && errno == EINTR) continue;
    else if (ret == -1 && errno == EPIPE) return PIPE_ERROR;
    else break;
  }
  if (LOG) printf("\nSEND_MSG:\t[%s]\n", senzaslashenne(msg));

  return ret;
}
//MSG_WAITALL
size_t recv_msg(int socket, char *buff, size_t buf_len) {
  int ret;
  while (1) {
    memset(buff, 0, buf_len);
    ret=recv(socket, buff, buf_len, 0 );
    if (ret==-1 && errno==EINTR) continue;
    else if (ret==-1 && errno==EWOULDBLOCK ) {
      return TIMEOUT_EXPIRED;
    }
    else break;
  }
  if (LOG) printf("\nRECV_MSG:\t[%s]\n", senzaslashenne(buff));
  return ret;
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
  res=recv_msg(socket, buff, buff_len);
  if (res==TIMEOUT_EXPIRED) return TIMEOUT_EXPIRED;
  return message_action(buff);
}

int send_and_parse(int socket, char* buff) {
  int res;
  send_msg(socket, buff);
  return message_action(buff);

}

char* crypt(char* msg){
  char * ris=malloc(MSG_SIZE*sizeof(char));
  SHA1Context sha;
  SHA1Reset(&sha);
  SHA1Input(&sha,msg,strlen(msg));
  SHA1Result(&sha);
  if (LOG) printf("hash calcolato del messaggio %s ricevuto: \n",msg );
  int i;
  for (i = 0; i < 5; i++) {
    if (LOG)  printf("%X ",sha.Message_Digest[i] );
  }
  sprintf(ris,"%X %X %X %X %X",sha.Message_Digest[0],sha.Message_Digest[1],sha.Message_Digest[2],sha.Message_Digest[3],sha.Message_Digest[4]);
  return ris;
}

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
