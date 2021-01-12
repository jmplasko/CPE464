#ifndef TRACEH
#define TRACEH
	struct Ethernet{
		u_char address[6];
		u_char source[6];
		u_short type;
	};

	struct Arp{
		uint16_t hardwaretype;   /* hardware address */
		uint16_t protocoltype;   /* protocol address */
		uint8_t hardwaresize;    /* Length of hardware */
		uint8_t protocolsize;    /* Length of protocol */
		uint16_t op;			 /* ARP opcode  */
		uint8_t sendersMAC[10];  /* Sender address */
		uint8_t targetMAC[10];   /* Target address */
	} ;

	struct IP {
		uint8_t versionlength; /* version and header length */
		uint8_t  dsf;		/* type of service */
		uint16_t  length;		/* total length */
		uint16_t  id;		/* identification */
		uint16_t  offset;		/* fragment offset field */
		uint8_t  ttl;		/* time to live */
		uint8_t  protocol;		/* protocol */
		uint16_t  sum;		/* checksum */
		uint32_t ip_src;
		uint32_t ip_dst; /* source and dest address */
	};

	struct TCP {
		uint16_t source;	/* source port */
		uint16_t dest;	/* destination port */
		uint32_t seq;		/* sequence number */
		uint32_t ack;		/* acknowledgement number */
		uint16_t flags;
		uint16_t win;		/* window */
		uint16_t sum;		/* checksum */
		uint16_t urp;		/* urgent pointer */
	};

	struct UDP {
		uint16_t source; /* source port */
		uint16_t dest;   /* destination port */
		uint16_t len;    /* length */
		uint16_t checksum;  /* checksum */
	};

	struct ICMP {
		uint8_t type;
	};

#endif
