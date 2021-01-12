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

#include "checksum.h"


uint8_t* createPDU(uint32_t sequenceNumber, uint8_t flag, uint8_t* payload, int dataLen) 
{
	static uint8_t pduBuffer[87];
	uint16_t checksum = 0;
	uint32_t seqOrder = htonl(sequenceNumber);

	memcpy(pduBuffer, &seqOrder, 4);
	pduBuffer[6] = flag;
	memcpy(pduBuffer + 7, payload, dataLen);

	checksum = in_cksum((short unsigned int* )pduBuffer, dataLen + 7);

	memcpy(pduBuffer + 4, &checksum, 2);


	return pduBuffer;
}

void outputPDU(uint8_t* aPDU, int pduLength)
{
	uint32_t sequenceNumber;
	uint8_t flag;
	uint8_t payload[81];

	memcpy(&sequenceNumber, aPDU, 4);
	sequenceNumber = ntohl(sequenceNumber);
	flag = aPDU[6];
	memcpy(payload, aPDU+7, pduLength-7);

	printf("SequenceNumber: %i\n", sequenceNumber);
	printf("Flag: %i\n", flag);
	printf("Payload: %s\n", payload);

}