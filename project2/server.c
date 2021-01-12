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

#include "community.h"
#include "gethostbyname6.h"
#include "networks.h"
#include "pollLib.h"

#define DEBUG_FLAG 1

void processSockets(int mainServerSocket);
void recvFromClient(int clientSocket);
void addNewClient(int mainServerSocket);
void removeClient(int clientSocket);
int checkArgs(int argc, char *argv[]);
void send_flag2or3(int socketNum, char* packet);
void flag4Case(int socketNum, char* packet);
void send_flag13(int socketNum);
void send_flag12(int socketNum, char* handle, int len);
void send_flag7(int socketNum, char* goal);
void flag5Case(int socketNum, char* packet);
void send_flag9(int socketNum);
void send_flag11(int socketNum);
void flag10Case(int socketNum);

linkedNode* head = NULL;

int main(int argc, char *argv[])
{
	int mainServerSocket = 0;   //socket descriptor for the server socket
	int portNumber = 0;
	
	setupPollSet();
	portNumber = checkArgs(argc, argv);
	
	//create the server socket
	mainServerSocket = tcpServerSetup(portNumber);

	// Main control process (clients and accept())
	processSockets(mainServerSocket);
	
	// close the socket - never gets here but nice thought
	close(mainServerSocket);
	
	return 0;
}

void processSockets(int mainServerSocket)
{
	int socketToProcess = 0;

	addToPollSet(mainServerSocket);
		
	while (1)
	{
		if ((socketToProcess = pollCall(POLL_WAIT_FOREVER)) != -1)
		{
			if (socketToProcess == mainServerSocket)
			{
				addNewClient(mainServerSocket);
			}
			else
			{
				recvFromClient(socketToProcess);
			}
		}

		//printf("%s", head->handleName);

	}
}


void recvFromClient(int clientSocket)
{
	char buf[MAXBUF];
	int messageLen = 0;
	uint8_t flag = 0;
		
	//now get the data from the clientSocket (message includes null)
	if ((messageLen = recv(clientSocket, buf, MAXBUF, 0)) < 0)
	{
		perror("recv call");
		exit(-1);
	}
	
	if (messageLen == 0)
	{
		// recv() 0 bytes so client is gone
		removeClient(clientSocket);
		head = removeHandle(head, clientSocket);
	}
	else
	{
		flag = buf[2];

		if (flag == 1) {
			send_flag2or3(clientSocket, buf);
		}
		else if (flag == 4) {
			flag4Case(clientSocket, buf);
		}
		else if (flag == 5) {
			flag5Case(clientSocket, buf);
		}
		else if (flag == 8) {
			send_flag9(clientSocket);
		}
		else if (flag == 10) {
			flag10Case(clientSocket);
		}
	}
}

void addNewClient(int mainServerSocket)
{
	int newClientSocket = tcpAccept(mainServerSocket, DEBUG_FLAG);
	
	addToPollSet(newClientSocket);
	
}

void removeClient(int clientSocket)
{
	printf("Client on socket %d terminted\n", clientSocket);
	removeFromPollSet(clientSocket);
	close(clientSocket);
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

void send_flag2or3(int socketNum, char* packet) {
	char packetReturn[MAXBUF];
	uint16_t pdu_len = htons(3);

	memcpy(packetReturn, &pdu_len, 2);

	char handle[101];
	uint8_t handleLen = 0;
	memcpy(&handleLen, packet + 3, 1);
	memcpy(handle, packet + 4, handleLen);
	packetReturn[handleLen] = 0;

	if (checkHandle(handle, head) == 1) {


		linkedNode* newClient = initializelinkedNode(socketNum);
		
		newClient->handleName = strdup(handle);
		newClient->socketNum = socketNum;
		newClient->next = head;
		
		head = newClient;

		packetReturn[2] = 2;

		check_send(socketNum, packetReturn, 3);
	}
	else {

		packetReturn[2] = 3;
		check_send(socketNum, packetReturn, ntohs(pdu_len));
	}
}

void flag4Case(int socketNum, char* packet) {
	char handle[101];
	uint16_t packetLen = 0;
	uint8_t handleLen = 0;
	linkedNode* current = head;
	memcpy(&packetLen, packet, 2);

	packetLen = ntohs(packetLen);

	memcpy(&handleLen, packet + 3, 1);
	memcpy(handle, packet + 4, handleLen);
	handle[handleLen] = 0;
	while (current != NULL) {
		if (strcmp(current->handleName, handle) != 0 && current->socketNum != socketNum) {
			check_send(current->socketNum, packet, (int)packetLen);
		}
		current = current->next;
	}
}

void flag5Case(int socketNum, char* packet) {
	char handle[101];
	uint16_t packetLen = 0;
	uint8_t handleLen = 0;
	uint8_t handleCount = 0;
	int offsetByte = 0;
	char goal[101];
	uint8_t currentHandleLen = 0;
	int invalidflag = 1;
	int i = 0;

	memcpy(&packetLen, packet, 2);
	packetLen = ntohs(packetLen);
	memcpy(&handleLen, packet + 3, 1);
	memcpy(handle, packet + 4, handleLen);
	handle[handleLen] = 0;
	memcpy(&handleCount, packet + 4 + handleLen, 1);
	
	
	offsetByte = 5 + handleLen;



	while (i < handleCount) {
		currentHandleLen = 0;
		linkedNode* current = head;
		invalidflag = 1;


		memcpy(&currentHandleLen, packet + offsetByte, 1);
		offsetByte += 1;
		memcpy(goal, packet + offsetByte, currentHandleLen);
		goal[currentHandleLen] = 0;
		offsetByte += currentHandleLen;
		while (current != NULL) {
			if (strcmp(current->handleName, goal) == 0) {
				check_send(current->socketNum, packet, (int)packetLen);
				invalidflag = 0;
			}
			current = current->next;
		}
		if (invalidflag) {
			send_flag7(socketNum, goal);
		}
		i++;
	}
}

void send_flag7(int socketNum, char* goal) {
	char packet[MAXBUF];
	uint8_t handleLen = strlen(goal);
	uint16_t packetLen = htons(4 + handleLen);

	memcpy(packet, &packetLen, 2);
	packet[2] = 7;
	memcpy(packet + 3, &handleLen, 1);
	memcpy(packet + 4, goal, handleLen);
	check_send(socketNum, packet, (int)ntohs(packetLen));
}

void send_flag9(int socketNum) {
	char packet[MAXBUF];
	uint16_t pdu_len = htons(3);

	memcpy(packet, &pdu_len, 2);
	packet[2] = 9;

	check_send(socketNum, packet, ntohs(pdu_len));
}

void flag10Case(int socketNum) {
	linkedNode* current = head;
	send_flag11(socketNum);

	while (current != NULL) {
		send_flag12(socketNum, current->handleName, strlen(current->handleName));
		current = current->next;
	}
	send_flag13(socketNum);
}

void send_flag11(int socketNum) {
	char packet[7];
	uint32_t numHandles = 0;
	uint16_t packetSize = htons(7);
	linkedNode* current = head;

	while (current != NULL) {
		numHandles++;
		current = current->next;
	}
	numHandles = htonl(numHandles);

	memcpy(packet, &packetSize, 2);
	packet[2] = 11;
	memcpy(packet + 3, &numHandles, 4);
	check_send(socketNum, packet, ntohs(packetSize));
}

void send_flag12(int socketNum, char* handle, int len) {
	uint8_t handleLen = len;
	char packet[MAXBUF];
	uint16_t packetSize = htons(4 + handleLen);

	memcpy(packet, &packetSize, 2);
	packet[2] = 12;
	memcpy(packet + 3, &handleLen, 1);
	memcpy(packet + 4, handle, len);
	check_send(socketNum, packet, ntohs(packetSize));
}

void send_flag13(int socketNum) {
	uint16_t packetSize = htons(3);
	char packet[3];

	memcpy(packet, &packetSize, 2);
	packet[2] = 13;
	check_send(socketNum, packet, ntohs(packetSize));
}