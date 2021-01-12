#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef __COMMUNITY__H__
#define __COMMUNITY__H__

typedef struct linkedNode {
	char* handleName;
	int socketNum;
	struct linkedNode* next;
} linkedNode;


linkedNode* removeHandle(linkedNode* head, int socketNum);
linkedNode* findNode(linkedNode* head, int socketNum);
linkedNode* initializelinkedNode(int socketNum);
//void addToList(linkedNode* linkedList, linkedNode* node);
int checkHandle(char* handle, linkedNode* head);

void check_send(int socketNum, char * packet, int packetLength);
int correct_recv(int socketNum, char * packet); 


#endif