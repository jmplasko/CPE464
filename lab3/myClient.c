/******************************************************************************
* myClient.c
*
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
#
void sendToServer(int socketNum);
void checkArgs(int argc, char * argv[]);

int main(int argc, char * argv[])
{
	int socketNum = 0;         //socket descriptor
	
	checkArgs(argc, argv);

	/* set up the TCP Client socket  */
	socketNum = tcpClientSetup(argv[1], argv[2], DEBUG_FLAG);
	
	sendToServer(socketNum);
	
	//close(socketNum);
	
	return 0;
}

void sendToServer(int socketNum)
{
	char sendBuf[MAXBUF];   //data buffer
	char aChar = 0;
	int sendLen = 0;        //amount of data to send
	int sent = 0;            //actual amount of data sent/* get the data and send it   */
	int twobytes = 0;

	// Important you don't input more characters than you have space 
	while (strcmp(sendBuf+2, "exit") != 0) {
		sendBuf[0] = 0;
		aChar = 0;
		sendLen = 2;        //amount of data to send

		printf("Enter data: ");
		while (sendLen < (MAXBUF - 1) && aChar != '\n')
		{
			aChar = getchar();
			if (aChar != '\n')
			{
				sendBuf[sendLen] = aChar;
				sendLen++;
			}
		}

		sendBuf[sendLen] = '\0';
		sendLen++;  //we are going to send the null
		
		twobytes = htons(sendLen);
		memcpy(&sendBuf, &twobytes, 2);

		printf("read: %s string len: %d (including null)\n", sendBuf, sendLen);

		sent = send(socketNum, sendBuf, sendLen, 0);
		if (sent < 0)
		{
			perror("send call");
			exit(-1);
		}

		printf("Amount of data sent is: %d\n", sent);
	}
}

void checkArgs(int argc, char * argv[])
{
	/* check command line arguments  */
	if (argc != 3)
	{
		printf("usage: %s host-name port-number \n", argv[0]);
		exit(1);
	}
}
