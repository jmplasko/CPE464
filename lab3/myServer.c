/******************************************************************************
* tcp_server.c
*
* CPE 464 - Program 1
*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#include "networks.h"

#define DEBUG_FLAG 1

void recvFromClient(int clientSocket);
int checkArgs(int argc, char *argv[]);

int main(int argc, char *argv[])
{
	int serverSocket = 0;   //socket descriptor for the server socket
	int clientSocket = 0;   //socket descriptor for the client socket
	int portNumber = 0;
	
	portNumber = checkArgs(argc, argv);
	
	//create the server socket
	serverSocket = tcpServerSetup(portNumber);

	// wait for client to connect
	while (1) {
		clientSocket = tcpAccept(serverSocket, DEBUG_FLAG);

		recvFromClient(clientSocket);

		/* close the sockets */
		close(clientSocket);
	}

	close(serverSocket);
	
	return 0;
}

void recvFromClient(int clientSocket)
{
	char buf[MAXBUF] = { 0 };
	int messageLen = 0;
	int flag = 0;
	int hold = 0;
	unsigned short lenmessage;
	
	while (strcmp(buf, "exit") != 0 && flag != 1) {
		// Use a time value of 1 second (so time is not null)
		while ((hold = selectCall(clientSocket, 1, 0, TIME_IS_NOT_NULL)) == 0)
		{
			printf("Select timed out waiting for client to send data\n");
		}

		//now get the data from the client_socket (message includes null)
		if ((messageLen = recv(clientSocket, &lenmessage, 2, MSG_WAITALL)) < 0)
		{
			perror("recv call");
			exit(-1);
		}

		if (messageLen == 0) {
			flag = 1;
		}
		else {

			lenmessage = ntohs(lenmessage);

			if ((messageLen = recv(clientSocket, buf, lenmessage - 2, MSG_WAITALL)) < 0)
			{
				perror("recv call");
				exit(-1);
			}

			if (messageLen == 0) {
				flag = 1;
			}
			else {
				printf("Recv Len: %d, Header Len: %d Data: %s\n", messageLen+2, messageLen, buf);
			}
		}
		
	}
}

int checkArgs(int argc, char *argv[])
{
	// Checks args and returns port number
	int portNumber = 0;

	if (argc > 2)
	{
		fprintf(stderr, "Usage %s [optional port number]\n", argv[0]);
		exit(-1);
	}
	
	if (argc == 2)
	{
		portNumber = atoi(argv[1]);
	}
	
	return portNumber;
}

