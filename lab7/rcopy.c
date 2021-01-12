/* rcopy - client in stop and wait protocol Writen: Hugh Smith */
// a bunch of #includes go here!
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
#include "cpe464.h"

#define SEQ_SIZE 4

typedef enum State STATE;

enum State
{
DONE, FILENAME, RECV_DATA, FILE_OK, START_STATE, SELECT_ONE
};

void processFile(char * argv[]);
STATE start_state(char ** argv, Connection * server, uint32_t * clientSeqNum);
STATE filename(char * fname, int32_t buf_size, Connection * server);
STATE recv_data(int32_t output_file, Connection * server, uint32_t * clientSeqNum, Window * clientWindow);
STATE file_ok(int * outputFileFd, char * outputFileName);
void check_args(int argc, char ** argv);

STATE select_one(Connection* server, Window* window, int* output_file);

void seqNum_equals_lower(int32_t output_file, uint8_t data_buf[MAX_LEN], int32_t data_len, int32_t seq_num, Window* window, Connection* server);
void seqNum_skip(Connection* server, int32_t seq_num, int32_t data_len, uint8_t data_buf[MAX_LEN], Window* window);
void only_one_missing(Window* window, Connection* server, uint8_t buf[MAX_LEN], int32_t data_len, uint32_t seq_num, int* output_file);
void multiple_missing(Window* window, Connection* server);

void send_response_packet(Connection* server, uint32_t seq_num, enum FLAG flag);



int main ( int argc, char *argv[] )
{
	check_args(argc, argv);
	sendtoErr_init(atof(argv[5]), DROP_ON, FLIP_ON, DEBUG_ON, RSEED_ON);
	processFile(argv);
	return 0;
}

void processFile(char * argv[])
{
	// argv needed to get file names, server name and server port number
	Connection server;
	uint32_t clientSeqNum = 0;
	int32_t output_file_fd = 0;
	STATE state = START_STATE;


	//create window - window, windowSize, bufferSize
	Window clientWindow;
	start_window(&clientWindow, atoi(argv[3]));
	printf("Created Window\n");


	while (state != DONE)
	{
		switch (state)
		{
			case START_STATE:
			state = start_state(argv, &server, &clientSeqNum);
			break;
			case FILENAME:
			state = filename(argv[1], atoi(argv[4]), &server);
			break;
			case FILE_OK:
			state = file_ok(&output_file_fd, argv[2]);
			break;
			case RECV_DATA:
			state = recv_data(output_file_fd, &server, &clientSeqNum, &clientWindow);
			break;
			case SELECT_ONE:
				state = select_one(&server, &clientWindow, &output_file_fd);
			break;
			case DONE:
				close(output_file_fd);
				free_window(&clientWindow);
			break;
			default:
			printf("ERROR - in default state\n");
			break;
		}
	}
}

STATE start_state(char ** argv, Connection * server, uint32_t * clientSeqNum)
{
	// Returns FILENAME if no error, otherwise DONE (to many connects, cannot connect to sever)
	uint8_t packet[MAX_LEN];
	uint8_t buf[MAX_LEN];
	int fileNameLen = strlen(argv[1]);
	STATE returnValue = FILENAME;
	uint32_t bufferSize = 0;
	uint32_t windowsize = 0;

	// if we have connected to server before, close it before reconnect
	if (server->sk_num > 0)
	{
		close(server->sk_num);
	}
	if (udpClientSetup(argv[6], atoi(argv[7]), server) < 0)
	{
		// could not connect to server
		returnValue = DONE;
	}
	else
	{
		// put in buffer size (for sending data) and filename
		bufferSize = htonl(atoi(argv[4]));
		windowsize = htonl(atoi(argv[3]));
		memcpy(buf, &bufferSize, SIZE_OF_BUF_SIZE);
		memcpy(buf+4, &windowsize, 4);
		memcpy(buf+8, argv[1], fileNameLen);
		printIPv6Info(&server->remote);
		send_buf(buf, fileNameLen + SIZE_OF_BUF_SIZE + 4, server, FNAME, *clientSeqNum, packet);
		(*clientSeqNum)++;
		returnValue = FILENAME;
	}
	return returnValue;
}

STATE filename(char * fname, int32_t buf_size, Connection * server)
{
	// Send the file name, get response
	// return START_STATE if no reply from server, DONE if bad filename, FILE_OK otherwise
	int returnValue = START_STATE;
	uint8_t packet[MAX_LEN];
	uint8_t flag = 0;
	uint32_t seq_num = 0;
	int32_t recv_check = 0;
	static int retryCount = 0;

	if ((returnValue = processSelect(server, &retryCount, START_STATE, FILE_OK, DONE)) ==
	FILE_OK)
	{
		recv_check = recv_buf(packet, MAX_LEN, server->sk_num, server, &flag, &seq_num);

		/* check for bit flip */
		if (recv_check == CRC_ERROR)
		{
			returnValue = START_STATE;
		}
		else if (flag == FNAME_BAD)
		{
			printf("File %s not found\n", fname);
			returnValue = DONE;
		}
		else if (flag == DATA)
		{
			// file yes/no packet lost - instead its a data packet
			returnValue = FILE_OK;
		}
	}
	return(returnValue);
}

STATE file_ok(int * outputFileFd, char * outputFileName)
{
	STATE returnValue = DONE;

	if ((*outputFileFd = open(outputFileName, O_CREAT | O_TRUNC | O_WRONLY, 0600)) < 0)
	{
		perror("File open error: ");
		returnValue = DONE;
	} else
	{
		returnValue = RECV_DATA;
	}
	
	return returnValue;
}

STATE recv_data(int32_t output_file, Connection * server, 
	uint32_t * clientSeqNum, Window* clientWindow)
{
	uint32_t seq_num = 0;
	uint8_t flag = 0;
	int32_t data_len = 0;
	uint8_t data_buf[MAX_LEN];
	uint8_t packet[MAX_LEN];

	if (select_call(server->sk_num, LONG_TIME, 0, NOT_NULL) == 0)
	{
		printf("Timeout after 10 seconds, server must be gone.\n");
		return DONE;
	}

	data_len = recv_buf(data_buf, MAX_LEN, server->sk_num, server, &flag, &seq_num);

	/* do state RECV_DATA again if there is a crc error (don't send ack, don't write data) */
	if (data_len == CRC_ERROR)
	{
		return RECV_DATA;
	}

	if (flag == END_OF_FILE)
	{
		/* send ACK */
		send_buf(packet, 1, server, EOF_ACK, *clientSeqNum, packet);
		(*clientSeqNum)++;
		printf("File done\n");
		return DONE;
	}
	else
	{

		//Send RR/SREJ
		//Slide window
		if (seq_num == clientWindow->lower) {
			seqNum_equals_lower(output_file, data_buf, data_len, seq_num, clientWindow, server);
			return RECV_DATA;
		}
		//Response from SREJ
		else if (seq_num < clientWindow->lower) {
			send_response_packet(server, seq_num, RR);
			return RECV_DATA;
		}
		else if (seq_num > clientWindow->upper) {
			return DONE;
		}
		else {
			seqNum_skip(server, seq_num, data_len, data_buf, clientWindow);
			return SELECT_ONE;
		}
	}
	return RECV_DATA;
}

void check_args(int argc, char ** argv)
{
	if (argc != 8)
	{
		printf("Usage %s fromFile toFile buffer_size window-size error_rate hostname port\n", argv[0]);
		exit(-1);
	}

	if (strlen(argv[1]) > 100)
	{
		printf("FROM filename to long needs to be less than 100 and is: %zu\n", strlen(argv[1]));
		exit(-1);
	}

	if (strlen(argv[2]) > 100)
	{
		printf("TO filename to long needs to be less than 100 and is: %zu\n", strlen(argv[1]));
		exit(-1);
	}

	if (atoi(argv[3]) < 1)
	{
		printf("Window size needs to be > 0 and is: %d\n", atoi(argv[3]));
		exit(-1);
	}

	if (atoi(argv[4]) < 400 || atoi(argv[4]) > 1400)
	{
		printf("Buffer size needs to be between 400 and 1400 and is: %d\n", atoi(argv[4]));
		exit(-1);
	}

	if (atoi(argv[5]) < 0 || atoi(argv[5]) >= 1 )
	{
		printf("Error rate needs to be between 0 and less then 1 and is: %d\n", atoi(argv[5]));
		exit(-1);
	}
}




void seqNum_equals_lower(int32_t output_file, uint8_t data_buf[MAX_LEN], int32_t data_len, int32_t seq_num, Window* window, Connection* server) {
	int i;
	int old_seqNum;
	queue_to_window(window, data_buf, data_len, seq_num);
	for (i = window->lower; i < window->upper; i++) {
		if (window->isOpen[i % window->window_size] == 0) {
			break;
		}
		dequeue_from_window(window, data_buf, &data_len, seq_num);
		write(output_file, data_buf, data_len);
	}
	just_slide(window, i);
	old_seqNum = window->lower;
	send_response_packet(server, old_seqNum, RR);
}

void seqNum_skip(Connection* server, int32_t seq_num, int32_t data_len, uint8_t data_buf[MAX_LEN], Window* window) {
	queue_to_window(window, data_buf, data_len, seq_num);
	window->current = window->lower;
	send_response_packet(server, seq_num, SREJ);
}

void send_response_packet(Connection* server, uint32_t seq_num, enum FLAG flag) { // RR and SREJ
	uint32_t temp1 = htonl(seq_num);
	uint8_t buf[MAX_LEN];
	uint8_t packet[MAX_LEN];
	memcpy(buf, &temp1, SEQ_SIZE);
	send_buf(buf, SEQ_SIZE, server, flag, seq_num, packet);
}

void only_one_missing(Window* window, Connection* server, uint8_t buf[MAX_LEN], int32_t data_len, uint32_t seq_num, int* output_file) {
	int i;
	uint8_t packet[MAX_LEN];
	queue_to_window(window, buf, data_len, seq_num);
	for (i = window->lower; i <= window->upper + 1; i++) {
		int index = i % window->window_size;
		if (window->isOpen[index] == 0) {
			window->current = i;
			break;
		}
	}
	for (i = window->lower; i < window->current; i++) {
		dequeue_from_window(window, packet, &data_len, seq_num);
		write(*output_file, packet, data_len);
	}
	just_slide(window, window->current);
	send_response_packet(server, window->lower, RR);
}

void multiple_missing(Window* window, Connection* server) {
	just_slide(window, window->current);
	send_response_packet(server, window->lower, RR);
}

STATE select_one(Connection* server, Window* window, int* output_file) {
	//int recv_check = 0;
	uint32_t crc_check = 0;
	uint32_t seq_num = 0;
	uint32_t lower = 0;
	uint32_t upper = 0;
	uint8_t buf[MAX_LEN];
	uint8_t flag = 0;
	static int retryCount2 = 0;
	STATE returnValue = DONE;

	if (retryCount2 > MAX_TRIES) {
		printf("Timeout after 10 seconds, server must be gone. \n");
		returnValue = DONE;
		return returnValue;
	}
	else {
		if (select_call(server->sk_num, SHORT_TIME, 0, NOT_NULL) == 0) {
			retryCount2++;
			returnValue = RECV_DATA;
			return returnValue;
		}
		else {
			//initialize back to 0
			retryCount2 = 0;
		}
	}
	crc_check = recv_buf(buf, MAX_LEN, server->sk_num, server, &flag, &seq_num);

	if (crc_check == CRC_ERROR) {
		returnValue = SELECT_ONE;
	}

	if (seq_num >= lower && seq_num <= upper) {
		only_one_missing(window, server, buf, crc_check, seq_num, output_file);
		returnValue = RECV_DATA;
	}
	else {
		multiple_missing(window, server);
		returnValue = SELECT_ONE;
	}
	return returnValue;

}