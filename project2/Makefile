# Makefile for CPE464 tcp test code
# written by Hugh Smith - April 2019

CC= gcc
CFLAGS= -g -Wall
LIBS = 


all:   cclient server

cclient: cclient.c networks.o pollLib.o gethostbyname6.o community.o *.h
	$(CC) $(CFLAGS) -o cclient cclient.c networks.o pollLib.o gethostbyname6.o community.o $(LIBS)

server: server.c networks.o pollLib.o gethostbyname6.o community.o *.h
	$(CC) $(CFLAGS) -o server server.c networks.o pollLib.o gethostbyname6.o community.o $(LIBS)

.c.o:
	gcc -c $(CFLAGS) $< -o $@ $(LIBS)

cleano:
	rm -f *.o

clean:
	rm -f myServer myClient *.o




