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
#include "srej.h"
#include "checksum.h"

int32_t send_buf(uint8_t * buf, uint32_t len, Connection * connection,
uint8_t flag, uint32_t seq_num, uint8_t * packet)
{
	int32_t sentLen = 0;
	int32_t sendingLen = 0;
	/* set up the packet (seq#, crc, flag, data) */
	if (len > 0)
	{
		memcpy(&packet[sizeof(Header)], buf, len);
	}
	sendingLen = createHeader(len, flag, seq_num, packet);
	sentLen = safeSendto(packet, sendingLen, connection);
	return sentLen;
}

int createHeader(uint32_t len, uint8_t flag, uint32_t seq_num, uint8_t * packet)
{
	// creates the regular header (puts in packet): seq num, flag, checksum
	Header * aHeader = (Header *) packet;
	uint16_t checksum = 0;
	seq_num = htonl(seq_num);
	memcpy(&(aHeader->seq_num), &seq_num, sizeof(seq_num));
	aHeader->flag = flag;
	memset(&(aHeader->checksum), 0, sizeof(checksum));
	checksum = in_cksum((unsigned short *) packet, len + sizeof(Header));
	memcpy(&(aHeader->checksum), &checksum, sizeof(checksum));
	return len + sizeof(Header);
}

int32_t recv_buf(uint8_t * buf, int32_t len, int32_t recv_sk_num,
Connection * connection, uint8_t * flag, uint32_t * seq_num)
{
	uint8_t data_buf[MAX_LEN];
	int32_t recv_len = 0;
	int32_t dataLen = 0;
	recv_len = safeRecvfrom(recv_sk_num, data_buf, len, connection);
	dataLen = retrieveHeader(data_buf, recv_len, flag, seq_num);
	// dataLen could be -1 if crc error or 0 if no data
	if (dataLen > 0) {
		memcpy(buf, &data_buf[sizeof(Header)], dataLen);
	}

	return dataLen;
}

int retrieveHeader(uint8_t * data_buf, int recv_len, uint8_t * flag, uint32_t * seq_num)
{
	Header * aHeader = (Header *) data_buf;
	int returnValue = 0;
	if (in_cksum((unsigned short *) data_buf, recv_len) != 0)
	{
		returnValue = CRC_ERROR;
	}
	else
	{
		*flag = aHeader->flag;
		memcpy(seq_num, &(aHeader->seq_num), sizeof(aHeader->seq_num));
		*seq_num = ntohl(*seq_num);
		returnValue = recv_len - sizeof(Header);
	}
	return returnValue;
}

int processSelect(Connection * client, int * retryCount,
int selectTimeoutState, int dataReadyState, int doneState)
{
	// Returns:
	// doneState if calling this function exceeds MAX_TRIES
	// selectTimeoutState if the select times out without receiving anything
	// dataReadyState if select() returns indicating that data is ready for read
	int returnValue = dataReadyState;
	(*retryCount)++;
	if (*retryCount > MAX_TRIES)
	{
		printf("No response for other side for %d seconds, terminating connection\n", MAX_TRIES);
		returnValue = doneState;
	}
	else
	{
		if (select_call(client->sk_num, SHORT_TIME, 0, NOT_NULL) == 1)
		{
			*retryCount = 0;
			returnValue = dataReadyState;
		}
		else
		{
			// no data ready
			returnValue = selectTimeoutState;
		}
	}
	return returnValue;
}

void start_window(Window* window, int32_t windowSize) {
	window->lower = 1;
	window->current = 1;
	window->upper = windowSize + 1;
	window->window_size = windowSize;
	window->buf = calloc(sizeof(WindowContent) * windowSize, sizeof(uint8_t));
	window->isOpen = calloc(sizeof(uint8_t) * windowSize, sizeof(uint8_t));
}

void queue_to_window(Window* window, uint8_t packet[MAX_LEN], int len, int seq_num) {
	int index = seq_num % window->window_size;
	memcpy((window->buf)[index].buf, packet, len);
	(window->buf)[index].packet_len = len;
	(window->buf)[index].seq_num = seq_num;
	(window->isOpen)[index] = 1;
}

void dequeue_from_window(Window* window, uint8_t packet[MAX_LEN], int32_t* len_read, int seq_num) {
	int index = seq_num % window->window_size;
	memcpy(packet, (window->buf)[index].buf, (window->buf)[index].packet_len);
	*len_read = (window->buf)[index].packet_len;
}

void delete_from_window(Window* window, int seq_num) {
	int i;
	int index;
	for (i = window->lower; i < seq_num; i++) {
		index = i % window->window_size;
		(window->isOpen)[index] = 0;
	}
}

void just_slide(Window* window, int seq_num) {
	int i;
	int index;
	for (i = window->lower; i < seq_num; i++) {
		index = i % window->window_size;
		(window->isOpen)[index] = 0;
	}
	window->upper += seq_num - window->lower;
	window->lower = seq_num;
	if (window->lower > window->current) {
		window->current = window->lower;
	}
}

void free_window(Window* window) {
	free(window->buf);
	free(window->isOpen);
}