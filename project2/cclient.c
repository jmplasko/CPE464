//
// Written Hugh Smith, Updated: April 2020
// Use at your own risk.  Feel free to copy, just leave my name in it.
//

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
#include  <ctype.h>

#include "community.h"
#include "gethostbyname6.h"
#include "networks.h"
#include "pollLib.h"


#define DEBUG_FLAG 1

void sendToServer(int socketNum, char * handle);
int getFromStdin(char * sendBuf);
void checkArgs(int argc, char * argv[]);
void sendOrReceive(char* handle, int socketNum);
void recvFromServer(int socketNum);
void fullsendmessage(int serverSocket, char* handle, char* packet, int packetSize, char* buf, int packetByteSize);

void flag_2or3(int socketNum);
void flag9();
void flag4(char * buf);
void flag5(char * buf);
void flag7(char * buf);
void flag111213(int socketNum, char* buf);

void sflag1(char* handle, int socketNum);
void sflag4(int socketNum, char* handle, char* text);
void sflag5(int socketNum, char *handle, char * buf);
void sflag8(int socketNum);
void sflag10(int socketNum);


int main(int argc, char * argv[])
{
	int socketNum = 0;         //socket descriptor
	
	setupPollSet();

	checkArgs(argc, argv);

	/* set up the TCP Client socket  */
	socketNum = tcpClientSetup(argv[2], argv[3], DEBUG_FLAG);

	sflag1(argv[1], socketNum);
	flag_2or3(socketNum);
	sendOrReceive(argv[1], socketNum);
	
	close(socketNum);

	return 0;
}

void sflag1(char* handle, int socketNum) {
	char packet[MAXBUF];
	uint8_t handleLen = strlen(handle);
	uint16_t pdulen = htons(3 + 1 + handleLen);

	memcpy(packet, &pdulen, 2);
	packet[2] = 1;
	memcpy(packet + 2 + 1, &handleLen, 1);
	memcpy(packet + 2 + 1 + 1, handle, handleLen);
		
	check_send(socketNum, packet, ntohs(pdulen));
}

void flag_2or3(int socketNum) {
	char packet[MAXBUF];
	int messageLength = 0;
	char handleName[101];
	uint8_t handleLen = 0;

	messageLength = correct_recv(socketNum, packet);	

	if (messageLength == 0) {
		printf("Server Terminated");
	}

	memcpy(&handleLen, packet + 3, 1);
	memcpy(&handleName, packet + 4, handleLen);
	handleName[handleLen] = 0;

	if (packet[2] == 3) {
		printf("Handle already in use: %s", handleName);
		close(socketNum);
		exit(1);
	}
	else if (packet[2] != 2) {
		printf("Error on initial packet: incorrect packet type");
		close(socketNum);
		exit(1);
	}
}

void sendOrReceive(char* handle, int socketNum)
{
	int socketToProcess = 0;

	addToPollSet(socketNum);
	addToPollSet(STDIN_FILENO);

	while (1)
	{

		printf("$: ");
		fflush(stdout);
		
		if ((socketToProcess = pollCall(POLL_WAIT_FOREVER)) != -1)
		{
			

			if (socketToProcess == STDIN_FILENO)
			{	
				sendToServer(socketNum, handle);
			}
			else
			{
				recvFromServer(socketNum);
			}
		}
	}
}

void recvFromServer(int socketNum)
{
	char buf[MAXBUF];
	int messageLength = 0;
	uint8_t flag = 0;

	messageLength = correct_recv(socketNum, buf);

	if (messageLength == 0)
	{
		printf("Server Terminated\n");
		close(socketNum);
		exit(1);
	}

	flag = buf[2];

	if (flag == 4) { 
		flag4(buf); 
	}else if (flag == 5) { 
		flag5(buf); 
	}else if (flag == 7) { 
		flag7(buf);
	}else if (flag == 9) {
		flag9(); 
	}else if (flag == 11) {
		flag111213(socketNum, buf);
	}

}

void sendToServer(int socketNum, char * handle)
{
	char stringtoToken[MAXBUF];
	char* tokenize = NULL;

	getFromStdin(stringtoToken);

	tokenize = strtok(stringtoToken, " ");

	if (tokenize != NULL) {

		if (tokenize[0] == '%') {
			if (tokenize[1] == 'm' || tokenize[1] == 'M') {
				sflag5(socketNum, handle, strtok(NULL, ""));
			}
			else if (tokenize[1] == 'e' || tokenize[1] == 'E') {
				sflag8(socketNum);
			}
			else if (tokenize[1] == 'b' || tokenize[1] == 'B') {
				char* text = strtok(NULL, "");
				if (text == NULL) {
					text = "\n";
				}
				sflag4(socketNum, handle, text);
			}
			else if (tokenize[1] == 'l' || tokenize[1] == 'L') {
				sflag10(socketNum);
			}
			else {
				printf("Invalid command\n");
			}
		}
		else {
			printf("Invalid command format\n");
		}
	}
}

int getFromStdin(char * sendBuf)
{
	// Gets input up to MAXBUF-1 (and then appends \0)
	// Returns length of string including null
	char aChar = 0;
	int inputLen = 0;       
	
	// Important you don't input more characters than you have space 
	while (inputLen < (MAXBUF - 1) && aChar != '\n')
	{
		aChar = getchar();
		if (aChar != '\n')
		{
			sendBuf[inputLen] = aChar;
			inputLen++;
		}
	}

	sendBuf[inputLen] = '\0';
	inputLen++;  //we are going to send the null
	
	return inputLen;
}

void checkArgs(int argc, char * argv[])
{
	/* check command line arguments  */
	if (argc != 4)
	{
		printf("usage: %s handle server-name server-port \n", argv[0]);
		exit(1);
	}
	else if (strlen(argv[1]) > 100) {
		printf("Invalid handle, handle longer than 100 characters: %s\n", argv[1]);
		exit(1);
	}
	else if (isdigit(argv[1][0])) {
		printf("Invalid handle, handle starts with a number\n");
		exit(1);
	}
}

void sflag8(int socketNum) {
	char packet[MAXBUF];
	uint16_t len_packet = htons(3);

	memcpy(packet, &len_packet, 2);
	packet[2] = 8;

	check_send(socketNum, packet, 3);
}

void flag9() {
	printf("Exiting\n");
	exit(1);
}

void sflag10(int socketNum) {
	char Packet[MAXBUF];
	uint16_t packetLen = htons(3);

	memcpy(Packet, &packetLen, 2);
	Packet[2] = 10;
	check_send(socketNum, Packet, 3);
}

void flag111213(int socketNum, char* buf) {
	uint32_t Handles = 0;
	char packet[MAXBUF];
	int packetLen = 0;
	uint8_t handleLen = 0;
	char handleName[101];

	int j = 0;
	memcpy(&Handles, buf + 3, 4);
	printf("Number of clients: %u\n", ntohl(Handles));

	while (j < ntohl(Handles)) {

		
		packetLen = correct_recv(socketNum, packet);

		if (packetLen == 0) {
			printf("Server Terminated\n");
			exit(1);
		}
		memcpy(&handleLen, packet + 3, 1);
		memcpy(handleName, packet + 4, handleLen);
		handleName[handleLen] = 0;
		printf("\t%s\n", handleName);

		j++;
	}

	packetLen = correct_recv(socketNum, packet);

	if (packetLen == 0) {
		printf("Server Terminated\n");
		close(socketNum);
		exit(1);
	}

	if (packet[2] != 13) {
		printf("Handle count request error\n");
	}
}

void flag7(char* buf) {
	uint8_t handleLen = 0;
	char handle[MAXBUF];

	memcpy(&handleLen, buf + 3, 1);
	memcpy(handle, buf + 4, handleLen);

	handle[handleLen] = 0;
	printf("Client with handle %s does not exist\n", handle);
}

void sflag5(int socketNum, char* handle, char* inputBuf) {
	char* token = NULL;
	char* text = NULL;
	token = strtok(inputBuf, " ");
	uint8_t handleCount = atoi(token);
	char sendBuf[MAXBUF];
	uint8_t handleLen;
	uint16_t packetLen;

	if (handleCount < 1 || handleCount > 9) {
		printf("Destination handle number error: 1-9\n");
	}

	if (handleCount == 0) {
		char packet[MAXBUF];
		char* target = token;
		uint8_t targetHandleLen = strlen(target);
		int packetSize = 0;
		int num = 0;
		int yetToGet;
		handleCount = 1;
		memcpy(packet, &handleCount, 1);
		memcpy(packet + 1, &targetHandleLen, 1);
		memcpy(packet + 1 * 2, target, (int)targetHandleLen);
		text = strtok(NULL, "");
		if (text == NULL) {
			text = "\n";
		}
		packetSize = packetSize + 1 + 1 + targetHandleLen;
		yetToGet = strlen(text);
		while (yetToGet > 0) {
			char inputBuf[201];
			int bytes = 0;
			if (yetToGet >= 200) {
				memcpy(inputBuf, text + num, 200);
				num += 200;
				yetToGet -= 200;
				bytes = 200;
			}
			else {
				memcpy(inputBuf, text + num, yetToGet);
				num += yetToGet;
				bytes = yetToGet;
				yetToGet = 0;
			}
			inputBuf[bytes] = 0;

			handleLen = strlen(handle);
			packetLen = htons(3 + 1 + handleLen + packetSize + bytes+1);

			memcpy(sendBuf, &packetLen, 2);
			sendBuf[2] = 5;
			memcpy(sendBuf + 2 + 1, &handleLen, 1);
			memcpy(sendBuf + 2 + 1 + 1, handle, handleLen);
			memcpy(sendBuf + 2 + 1 + 1 + handleLen, packet, packetSize);
			memcpy(sendBuf + 2 + 1 + 1 + handleLen + packetSize, inputBuf, bytes+1);
			check_send(socketNum, sendBuf, ntohs(packetLen));
		}
	}
	else {
		int i = 0;
		int offset = 0;
		int num = 0;
		int yetToGet;
		char packet[MAXBUF];
		memcpy(packet, &handleCount, 1);
		offset += 1;
		while (i < handleCount) {
			char* dest = strtok(NULL, " ");
			uint8_t targetSize = strlen(dest);
			memcpy(packet + offset, &targetSize, 1);
			offset += 1;
			memcpy(packet + offset, dest, (int)targetSize);
			offset += targetSize;
			i++;
		}
		text = strtok(NULL, "");
		if (text == NULL) {
			text = "\n";
		}
		yetToGet = strlen(text);
		while (yetToGet > 0) {
			char inputBuf[201];
			int bytes = 0;
			if (yetToGet >= 200) {
				memcpy(inputBuf, text + num, 200);
				num += 200;
				yetToGet -= 200;
				bytes= 200;
			}
			else {
				memcpy(inputBuf, text + num, yetToGet);
				num += yetToGet;
				bytes = yetToGet;
				yetToGet = 0;
			}
			inputBuf[bytes] = 0;
			bytes++;

			handleLen = strlen(handle);
			packetLen = htons(3 + 1 + handleLen + offset + bytes);

			memcpy(sendBuf, &packetLen, 2);
			sendBuf[2] = 5;
			memcpy(sendBuf + 2 + 1, &handleLen, 1);
			memcpy(sendBuf + 2 + 1 + 1, handle, handleLen);
			memcpy(sendBuf + 2 + 1 + 1 + handleLen, packet, offset);
			memcpy(sendBuf + 2 + 1 + 1 + handleLen + offset, inputBuf, bytes);
			check_send(socketNum, sendBuf, ntohs(packetLen));
		}
	}
}

void flag5(char* packet) {
	uint8_t handleLen = 0;
	char handle[101];
	uint8_t Handles = 0;
	int offset = 0;
	uint8_t goal = 0;
	int i = 0;
	int pint;

	memcpy(&handleLen, packet + 3, 1);
	offset = offset + 4;
	pint = (int)handleLen;

	memcpy(handle, packet + 4, pint);
	handle[handleLen] = 0;
	offset += handleLen;

	memcpy(&Handles, packet + offset, 1);
	offset += 1;

	while (i < Handles) {
		memcpy(&goal, packet + offset, 1);
		offset += goal + 1;
		i++;
	}
	packet = packet + offset;
	printf("\n%s: %s\n", handle, packet);
}

void sflag4(int socketNum, char* handle, char* text) {
	uint8_t handleLen;
	uint16_t packetLen;
	char packet[MAXBUF];
	int count = 0;
	int characters = strlen(text);
	char tempBuf[201];
	int bytesSize = 0;

	while (characters > 0) {
		bytesSize = 0;

		if (characters < 200) {
			memcpy(tempBuf, text + count, characters);
			count += characters;
			bytesSize = characters;
			characters = 0;
		}
		else {
			memcpy(tempBuf, text + count, 200);
			count += 200;
			characters -= 200;
			bytesSize = 200;
		}
		tempBuf[bytesSize] = 0;
		bytesSize++;
		handleLen = strlen(handle);
		packetLen = htons(4 + handleLen + bytesSize);

		memcpy(packet, &packetLen, 2);
		packet[2] = 4;
		memcpy(packet + 3, &handleLen, 1);
		memcpy(packet + 4, handle, handleLen);
		memcpy(packet + 4 + handleLen, tempBuf, bytesSize);
		check_send(socketNum, packet, ntohs(packetLen));
	}
}

void flag4(char* buf) {
	char handleName[101];
	uint8_t handleLen = 0;

	memcpy(&handleLen, buf + 3, 1);
	memcpy(&handleName, buf + 4, handleLen);
	printf("\n%s: %s\n", handleName, buf + 4 + handleLen);
}