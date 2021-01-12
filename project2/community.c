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
#include <arpa/inet.h>
#include <ctype.h>

#include "community.h"


int checkHandle(char* handle, linkedNode* head) {

	linkedNode* current = head;

	while (current != NULL) {
		if (current->handleName != NULL && strcmp(current->handleName, handle) == 0) {
			return -1;
		}
		else { current = current->next; }
	}
	return 1;
}

/*void addToList(linkedNode* head, linkedNode* newHead) {
	if (head == NULL) { - = node; }
	else {
		linkedNode* current = linkedList->head;
		while (current->next != NULL) {
			current = current->next;
		}
		current->next = node;
	}

	newHead->next = head;

}*/

linkedNode* initializelinkedNode(int socketNum) {
	linkedNode* node = malloc(sizeof(linkedNode));
	if (node == NULL) {
		printf("Error trying to malloc for a LinkedNode\n");
		exit(1);
	}
	else {
		node->handleName = NULL;
		node->socketNum = socketNum;
		node->next = NULL;
	}
	return node;
}


linkedNode* findNode(linkedNode* head, int socketNum) {
	linkedNode* current = head;
	while (current != NULL) {
		if (current->socketNum == socketNum) {
			return current;
		}
		current = current->next;
	}
	return NULL;
}

linkedNode* removeHandle(linkedNode* head, int socketNum) {
	linkedNode* current = head;
	linkedNode* newHead = head;
	linkedNode* prev = NULL;

	while (current != NULL) {

		if ((current->socketNum) == socketNum) {

			if (prev == NULL) {
				newHead = current->next;

				if (current->handleName != NULL) {
					free(current->handleName);
				}
				free(current);

				return newHead;

			}
			else {
				prev->next = current->next;
				newHead = prev;

				if (current->handleName != NULL) {
					free(current->handleName);
				}
				free(current);

				return newHead;
			}
			
		}

		prev = current;
		current = current->next;

	}

	return newHead;

}

void check_send(int socketNum, char * packet, int packetLength){
	if (send(socketNum, packet, packetLength, 0) < 0){
		printf("Packet error when sending!");
		exit(1);
	}
}

int correct_recv(int socketNum, char * packet) {
	int receive = 0;
	uint16_t packetLength = 0;
	receive = recv(socketNum, packet, 2, 0);

	if (receive < 0) {
		printf("recv() error: receiving packet error");
		exit(1);
	}

	if (receive == 0) {
		return packetLength;
	}

	memcpy(&packetLength, packet, 2);
	packetLength= ntohs(packetLength);

	int hold = receive;

	while ((packetLength - hold) > 0) {
		if ((receive = recv(socketNum, packet + hold, packetLength-hold, 0)) < 0) {
			printf("recv() error: receiving packet error");
			exit(1);
		}
		hold += receive;
	}

	return packetLength;
}
