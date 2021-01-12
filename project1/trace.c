/*Program 1 Trace by James Plasko*/

#include <stdio.h>
#include <pcap.h>
#include <stdlib.h>
#include <string.h> 
#include <netinet/ether.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdint.h>
#include "trace.h"
#include "checksum.h"


int main(int argc, char *argv[]){
	pcap_t *f;
	char filename[240];
	char errbuf[PCAP_ERRBUF_SIZE];
	struct pcap_pkthdr *header;
	const u_char *pktdata;
	char* holder;
	int count = 1;
	uint16_t check;
	const struct Ethernet* ethernet;
	const struct Arp* arp;
	struct IP* ip;
	struct TCP* tcp;
	struct UDP* udp;
	const struct ICMP* icmp;
	struct in_addr need;

	/* opening files and checking */
	if(argc != 2){
		printf("./trace [filename]");
		exit(0);
	}

	strcpy(filename, argv[1]);


	/* opening file for packets */
	if(!(f = pcap_open_offline(filename, errbuf))){
		fprintf(stderr, "error");
		exit(1);
	}

	/* goes through the packets */
	while (pcap_next_ex(f, &header, &pktdata) >= 0) {
		
		//puts information into ethernet struct
		ethernet = (struct Ethernet*)(pktdata);

		printf("\nPacket number: %d  Frame Len: %d\n\n", count, header->len);

		printf("	Ethernet Header\n");

		//converting the address to the correct string format
		holder = ether_ntoa((struct ether_addr*) &ethernet->address);
		printf("		Dest MAC: %s\n", holder);
		//converting the source to the correct string format
		holder = ether_ntoa((struct ether_addr*) &ethernet->source);
		printf("		Source MAC: %s\n", holder);
		
		//checking for type of packet
		if (ethernet->type == 1544) {
			printf("\t\tType: ARP\n\n");
			
			arp = (struct Arp*)(pktdata + 14);

			printf("\tARP header\n");

			if (htons(arp->op) == 1) {
				printf("\t\tOpcode: Request\n");
			}
			else {
				printf("\t\tOpcode: Reply\n");
			}
			
			//converting the address to the correct string format
			holder = ether_ntoa((struct ether_addr*) &arp->sendersMAC);
			printf("\t\tSender MAC: %s\n", holder);
			//converting the address to the correct string format
			printf("\t\tSender IP: %d.%d.%d.%d\n", arp->sendersMAC[6], 
												arp->sendersMAC[7],
												arp->sendersMAC[8],
												arp->sendersMAC[9]);
			

			//converting the source to the correct string format
			holder = ether_ntoa((struct ether_addr*) &arp->targetMAC);
			printf("\t\tTarget MAC: %s\n", holder);
			//converting the source to the correct string format
			printf("\t\tTarget IP: %d.%d.%d.%d\n\n", arp->targetMAC[6], 
													arp->targetMAC[7], 
													arp->targetMAC[8], 
													arp->targetMAC[9]);

		/* IP stuff */
		}else if(htons(ethernet->type) == 2048) {
			printf("\t\tType: IP\n\n");

			ip = (struct IP*)(pktdata + 14);
			
			printf("\tIP Header\n");

			printf("\t\tHeader Len: %d (bytes)\n", (ip->versionlength & 0x0F) * 4);
			
			printf("\t\tTOS: 0x%x\n", ip->dsf);

			printf("\t\tTTL: %d\n", ip->ttl);

			printf("\t\tIP PDU Len: %d (bytes)\n", ntohs(ip->length));

			if (ip->protocol == 6) {
				printf("\t\tProtocol: TCP\n");
			}
			else if (ip->protocol == 17) {
				printf("\t\tProtocol: UDP\n");
			}
			else if (ip->protocol == 1) {
				printf("\t\tProtocol: ICMP\n");
			}
			else{
				printf("\t\tProtocol: Unknown\n");
			}

			/* checksum check */
			check = in_cksum((short unsigned int *)(pktdata + 14), (ip->versionlength & 0x0F) * 4);

			if (check == 0) {
				printf("\t\tChecksum: Correct (0x%x)\n", ip->sum);
			}
			else {
				printf("\t\tChecksum: Incorrect (0x%x)\n", ip->sum);
			}
			//converting the address to the correct string format
			ip->ip_src = ntohl(ip->ip_src);
			need.s_addr = htonl(ip->ip_src);
			holder = inet_ntoa(need);
			printf("\t\tSender IP: %s\n", holder);

			//converting the source to the correct string format
			ip->ip_dst = ntohl(ip->ip_dst);
			need.s_addr = htonl(ip->ip_dst);
			holder = inet_ntoa(need);
			printf("\t\tDest IP: %s\n", holder);

			/* ICMP */
			if (ip->protocol == 1) {
				icmp = (struct ICMP*)((pktdata + 14) + (ip->versionlength & 0x0F) * 4);
				printf("\n\tICMP Header\n");
				
				if (icmp->type == 0) {
					printf("\t\tType: Reply\n");
				}else if (icmp->type == 109) {
					printf("\t\tType: 109\n");
				}
				else {
					printf("\t\tType: Request\n");
				}
				

			}/* TCP */
			else if (ip->protocol == 6) {

				tcp = (struct TCP*)((pktdata + 14) + (ip->versionlength & 0x0F) * 4);
				
				uint16_t lenhold = ntohs(ip->length);
				uint32_t srchld = ntohl(ip->ip_src);
				uint32_t desthld = ntohl(ip->ip_dst);

				/* this creates pseudo header with tcp pdu added on */
				u_char pseu[12 + (lenhold - (ip->versionlength & 0x0F) * 4)];
				memset(pseu, 0, 12 + (lenhold - (ip->versionlength & 0x0F) * 4));

				u_char vl[2];

				vl[0] = ((lenhold - (ip->versionlength & 0x0F) * 4) & 0xFF00) >> 8 ;
				vl[1] = (lenhold - (ip->versionlength & 0x0F) * 4) & 0x00FF;

				memcpy(pseu, &srchld, 4);
				memcpy(pseu+4, &desthld, 4);
				pseu[9] = 6;
				memcpy(pseu+10, vl, 2);
				memcpy(pseu+12, pktdata + 14 + (ip->versionlength & 0x0F) * 4, 
									(lenhold - (ip->versionlength & 0x0F) * 4));

				/* set original checksum to  zero */
				pseu[12 + 16] = 0;
				pseu[12 + 17] = 0;

				printf("\n\tTCP Header\n");

				if (ntohs(tcp->source) == 80) {
					printf("\t\tSource Port:  HTTP\n");
				}
				else {
					printf("\t\tSource Port: : %d\n", ntohs(tcp->source));
				}

				if (ntohs(tcp->dest) == 80) {
					printf("\t\tDest Port:  HTTP\n");
				}
				else {
					printf("\t\tDest Port: : %d\n", ntohs(tcp->dest));
				}

				printf("\t\tSequence Number: %u\n", ntohl(tcp->seq));

				if (htonl(tcp->ack) == 0 || (((ntohs(tcp->flags) & 0x00F0) % 0x0020) != 0x0010)) {
					printf("\t\tACK Number: <not valid>\n");
				}else {
					printf("\t\tACK Number: %u\n", htonl(tcp->ack));
				}


				if (((ntohs(tcp->flags) & 0x00F0) % 0x0020) == 0x0010) {
					printf("\t\tACK Flag: Yes\n");
				}
				else {
					printf("\t\tACK Flag: No\n");
				}
				if (((ntohs(tcp->flags) & 0x000F) % 0x004) >= 0x0002) {
					printf("\t\tSYN Flag: Yes\n");
				}
				else {
					printf("\t\tSYN Flag: No\n");
				}

				if (((ntohs(tcp->flags) & 0x000F) % 0x008) >= 0x0004) {
					printf("\t\tRST Flag: Yes\n");
				}
				else {
					printf("\t\tRST Flag: No\n");
				}

				if (((ntohs(tcp->flags) & 0x000F) % 0x002) == 0x0001) {
					printf("\t\tFIN Flag: Yes\n");
				}
				else {
					printf("\t\tFIN Flag: No\n");
				}

				printf("\t\tWindow Size: %d\n", ntohs(tcp->win));

				/* checksum check */
				check = in_cksum((short unsigned int*)pseu, 12 + (lenhold - (ip->versionlength & 0x0F) * 4));

				if (check == tcp->sum) {
					printf("\t\tChecksum: Correct (0x%x)\n", ntohs(tcp->sum));
				}
				else {
					printf("\t\tChecksum: Incorrect (0x%x)\n", ntohs(tcp->sum));
				}
			/* UDP */
			}else if (ip->protocol == 17) {

				udp = (struct UDP*)(pktdata + 14 + (ip->versionlength & 0x0F) * 4);

				printf("\n\tUDP Header\n");

				printf("\t\tSource Port: : %d\n", ntohs(udp->source));

				printf("\t\tDest Port: : %d\n", ntohs(udp->dest));
			}
			
		}

		//increasing packet number
		count++;

	}

	return 0;
}