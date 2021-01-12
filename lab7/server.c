/* Server stop and wait code - Writen: Hugh Smith */
// bunch of #includes go here
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
#include <sys/wait.h>
#include "networks.h"
#include "srej.h"
#include "cpe464.h"

typedef enum State STATE;

enum State
{
START, DONE, FILENAME, SEND_DATA, WAIT_ON_RR, TIMEOUT_ON_ACK,
WAIT_ON_EOF_ACK, TIMEOUT_ON_EOF_ACK
};

void process_server(int serverSocketNumber, float errorRate);
void process_client(int32_t serverSocketNumber, uint8_t * buf, int32_t recv_len, Connection *
client);
STATE filename(Connection* client, uint8_t* buf, int32_t recv_len, int32_t* data_file, int32_t* buf_size, Window* serverWindow);
STATE send_data(Connection * client, uint8_t * packet, int32_t * packet_len, int32_t data_file,
int32_t buf_size, uint32_t * seq_num, Window* serverWindow);
STATE timeout_on_ack(Connection * client, uint8_t * packet, int32_t packet_len);
STATE timeout_on_eof_ack(Connection * client, uint8_t * packet, int32_t packet_len);
STATE wait_on_rr(Connection * client, Window* window, int eof_bit, int32_t serverSocketNumber);
STATE wait_on_eof_ack(Connection * client);
int processArgs(int argc, char ** argv);

int main ( int argc, char *argv[])
{
	int32_t serverSocketNumber = 0;
	int portNumber = 0;
	portNumber = processArgs(argc, argv);
	sendtoErr_init(atof(argv[1]), DROP_ON, FLIP_ON, DEBUG_ON, RSEED_ON);
	/* set up the main server port */
	serverSocketNumber = udpServerSetup(portNumber);
	process_server(serverSocketNumber, atof(argv[1]));
	return 0;
}

void process_server(int serverSocketNumber, float errorRate)
{
	pid_t pid = 0;
	int status = 0;
	uint8_t buf[MAX_LEN];
	Connection client;
	uint8_t flag = 0;
	uint32_t seq_num =0;
	int32_t recv_len = 0;
	// get new client connection, fork() child, parent processes nonblocking waitpid()
	while (1)
	{
		// block waiting for a new client
		if (select_call(serverSocketNumber, LONG_TIME, 0, NOT_NULL) == 1)
		{
			recv_len = recv_buf(buf, MAX_LEN, serverSocketNumber, &client, &flag, &seq_num);
			if (recv_len != CRC_ERROR)
			{
				if ((pid = fork()) < 0)
				{
					perror("fork");
					exit(-1);
				}
				if (pid == 0)
				{
					// child process - a new process for each client
					printf("Child fork() - child pid: %d\n", getpid());
					sendtoErr_init(errorRate, DROP_ON, FLIP_ON, DEBUG_ON, RSEED_ON);
					process_client(serverSocketNumber, buf, recv_len, &client);
					exit(0); 
				}
			}
		}
		// check to see if any children quit (only get here in the parent process)
		while (waitpid(-1, &status, WNOHANG) > 0)
		{
		}
	}
}

void process_client(int32_t serverSocketNumber, uint8_t* buf, int32_t recv_len, Connection*
	client)
{
	STATE state = START;
	int32_t data_file = 0;
	int32_t packet_len = 0;
	uint8_t packet[MAX_LEN];
	int32_t buf_size = 0;
	uint32_t seq_num = START_SEQ_NUM;
	Window serverWindow;
	int eof_bit = 0;
	while (state != DONE)
	{
		switch (state)
		{
		case START:
			state = FILENAME;
			break;
		case FILENAME:
			state = filename(client, buf, recv_len, &data_file, &buf_size, &serverWindow);
			break;
		case SEND_DATA:
			state = send_data(client, packet, &packet_len, data_file, buf_size, &seq_num, &serverWindow);
			break;
		case WAIT_ON_RR:
			state = wait_on_rr(client, &serverWindow, eof_bit, serverSocketNumber);
			break;
		case TIMEOUT_ON_ACK:
			state = timeout_on_ack(client, packet, packet_len);
			break;
		case WAIT_ON_EOF_ACK:
			state = wait_on_eof_ack(client);
			break;
		case TIMEOUT_ON_EOF_ACK:
			state = timeout_on_eof_ack(client, packet, packet_len);
			break;
		case DONE:
			break;
		default:
			printf("In default and you should not be here!!!!\n");
			state = DONE;
			break;
		}
	}
}

STATE filename(Connection * client, uint8_t * buf, int32_t recv_len, int32_t * data_file, int32_t * buf_size, Window* serverWindow)
{
	uint8_t response[1];
	int32_t window_size;
	int32_t buffthings;
	char fname[MAX_LEN] = { 0 };
	STATE returnValue = DONE;

	// extract buffer sized used for sending data and also filename
	memcpy(&buffthings, buf, SIZE_OF_BUF_SIZE);
	buffthings = ntohl(buffthings);

	*buf_size = buffthings;

	memcpy(fname, &buf[sizeof(*buf_size) + sizeof(window_size)], recv_len - SIZE_OF_BUF_SIZE - 4);


	memcpy(&window_size, buf+4, 4);
	window_size = ntohl(window_size);

	/* Create client socket to allow for processing this particular client */
	client->sk_num = safeGetUdpSocket();


	if (((*data_file) = open(fname, O_RDONLY)) < 0)
	{
		send_buf(response, 0, client, FNAME_BAD, 0, buf);
		returnValue = DONE;
	}
	else
	{
		send_buf(response, 0, client, FNAME_OK, 0, buf);
		start_window(serverWindow, window_size);
		printf("Created Window\n");
		fflush(stdout);
		returnValue = SEND_DATA;
	}
	return returnValue;
}

STATE send_data(Connection* client, uint8_t* packet, int32_t* packet_len, int32_t data_file,
	int32_t buf_size, uint32_t* seq_num, Window* serverWindow)
{
	uint8_t buf[MAX_LEN];
	int32_t len_read = 0;
	STATE returnValue = DONE;

	len_read = read(data_file, buf, buf_size);

	switch (len_read)
	{
	case -1:
		perror("send_data, read error");
		returnValue = DONE;
		break;
	case 0:
		(*packet_len) = send_buf(buf, 1, client, END_OF_FILE, *seq_num, packet);
		returnValue = WAIT_ON_EOF_ACK;
		break;
	default:
		queue_to_window(serverWindow, buf, len_read, *seq_num);
		(*packet_len) = send_buf(buf, len_read, client, DATA, *seq_num, packet);
		(*seq_num)++;
		returnValue = WAIT_ON_RR;
		break;
	}
	return returnValue;
}

STATE wait_on_rr(Connection* client, Window* window, int eof_bit, int32_t serverSocketNumber)
{
	STATE returnValue = DONE;
	uint32_t crc_check = 0;
	uint8_t buf[MAX_LEN];
	int32_t len = MAX_LEN;
	uint8_t flag = 0;
	uint32_t seq_num = 0;
	static int retryCount = 0;
	uint8_t packet[MAX_LEN];
	int buf_size = 0;

	if ((returnValue = processSelect(client, &retryCount, TIMEOUT_ON_ACK, SEND_DATA, DONE)) ==
		SEND_DATA)
	{
		crc_check = recv_buf(buf, len, client->sk_num, client, &flag, &seq_num);

		if (crc_check == CRC_ERROR) {
			returnValue = WAIT_ON_RR;
		}else if (flag == RR) {
			if (eof_bit == 1) {
				return DONE;
			}
			if (seq_num > window->lower) {
				//printf("Received RR from rcopy\n");
				just_slide(window, seq_num);
			}
		}
		else if (flag == SREJ) {
			printf("SREJ received, Send again\n");
			dequeue_from_window(window, buf, &buf_size, seq_num);
			send_buf(buf, buf_size, client, DATA, seq_num, packet);
		}
		else if (flag != RR && flag != SREJ) {
			printf("This should never happen!\n");
			returnValue = DONE;
		}
		return returnValue;
	}
	return returnValue;
}

STATE wait_on_eof_ack(Connection* client)
{
	STATE returnValue = DONE;
	uint32_t crc_check = 0;
	uint8_t buf[MAX_LEN];
	int32_t len = MAX_LEN;
	uint8_t flag = 0;
	uint32_t seq_num = 0;
	static int retryCount = 0;
	if ((returnValue = processSelect(client, &retryCount, TIMEOUT_ON_EOF_ACK, DONE, DONE)) ==
		DONE)
	{
		crc_check = recv_buf(buf, len, client->sk_num, client, &flag, &seq_num);
		// if crc error ignore packet
		if (crc_check == CRC_ERROR)
		{
			returnValue = WAIT_ON_EOF_ACK;
		}
		else if (flag != EOF_ACK)
		{
			printf("In wait_on_eof_ack but its not an EOF_ACK flag (this should never happen) is: % d\n", flag);
				returnValue = DONE;
		}
		else
		{
			printf("File transfer completed successfully.\n");
			returnValue = DONE;
		}
	}
	return returnValue;
}

STATE timeout_on_ack(Connection* client, uint8_t* packet, int32_t packet_len)
{
	safeSendto(packet, packet_len, client);
	return WAIT_ON_RR;
}

STATE timeout_on_eof_ack(Connection* client, uint8_t* packet, int32_t packet_len)
{
	safeSendto(packet, packet_len, client);
	return WAIT_ON_EOF_ACK;
}

int processArgs(int argc, char** argv)
{
	int portNumber = 0;
	if (argc < 2 || argc > 3)
	{
		printf("Usage %s error_rate [port number]\n", argv[0]);
		exit(-1);
	}
	if (argc == 3)
	{
		portNumber = atoi(argv[2]);
	}
	else
	{
		portNumber = 0;
	}
	return portNumber;
}