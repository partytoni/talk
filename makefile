CC     = gcc
CFLAGS = -O3
NOWFLAG= -w
TFLAGS = -lpthread

all: client server

debug: client_debug server_debug

server: server.o util.o
	$(CC) -o server server.o $(CFLAGS) $(NOWFLAG) $(TFLAGS)

client: client.o util.o
	$(CC) -o client client.o $(CFLAGS) $(TFLAGS) $(NOWFLAG)

client.o: client.c
	$(CC) -c client.c util.c $(CFLAGS) $(TFLAGS) $(NOWFLAG)

server.o: server.c
	$(CC) -c server.c util.c $(CFLAGS) $(TFLAGS) $(NOWFLAG)

server_debug: server_debug.o util.o
	$(CC) -o server server.o $(CFLAGS) $(NOWFLAG) $(TFLAGS)

client_debug: client_debug.o util.o
	$(CC) -o client client.o $(CFLAGS) $(TFLAGS) $(NOWFLAG)

client_debug.o: client.c
	$(CC) -c client.c util.c $(CFLAGS) $(TFLAGS) $(NOWFLAG) -DDEBUG

server_debug.o: server.c
	$(CC) -c server.c util.c $(CFLAGS) $(TFLAGS) $(NOWFLAG) -DDEBUG

.phony: clean

clean:
	rm -f *.o server client
