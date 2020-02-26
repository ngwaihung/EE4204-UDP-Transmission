/* udpClient.c: Client to send 1 DU, wait for ACK, send 2 DU, wait for ACK, repeat
 *
 * Data to record: Transfer time, throughput for various sizes of data units
 * Flag to switch between alternating and S&W mode found in headsock.h
 *
 * NG WAI HUNG A0155269W
*/

#include "headsock.h"
#include <arpa/inet.h>
#include <stdbool.h>

float str_cli(FILE *fp, int sockfd, struct sockaddr *addr, int addrlen, long *len);
void tv_sub(struct timeval *out, struct timeval *in);
bool validIP(char *ipAddr);

int main(int argc, char **argv)
{
	int sockfd, ret;
	float ti, rt;
	long len;
	struct sockaddr_in ser_addr;
	char ** pptr;
	struct hostent *sh;
	struct in_addr **addrs;
	FILE *fp;

	// Check for presence of an input argument
	if (argc != 2) {
		printf("Parameters mismatch: Please enter a IPv4 address as argument\n");
		printf("E.G: ./udp_client4 127.0.0.1\n");
	}
	
	// Validate the input argument's IPv4 address
	if (!validIP(argv[1])) {
		printf("Please enter a valid IPv4 address: 0.0.0.0 ~ 255.255.255.255\n");
		printf("Avoid using a domain name if possible\n");
		exit(0);
	}

	// Receive and catch erroneous host information from input argument
	sh = gethostbyname(argv[1]); 
	if (sh == NULL) {
		printf("Error with gethostbyname");
		exit(0);
	}

	// Output host information
	/*
	printf("Canonical name: %s\n", sh->h_name);
	for (pptr=sh->h_aliases; *pptr != NULL; pptr++)
		printf("The aliases name is: %s\n", *pptr);
	switch(sh->h_addrtype)
	{
		case AF_INET:
			printf("AF_INET\n");
		break;
		default:
			printf("Unknown addrtype\n");
		break;
	}
	*/
    
	// Receive the server's IP address and create the socket
	addrs = (struct in_addr **)sh->h_addr_list;
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
	{
		printf("Error in socket");
		exit(1);
	}
	
	// Match the address family to IPv4 format
	ser_addr.sin_family = AF_INET;
	
	// Specify the port number in big endian format
	ser_addr.sin_port = htons(MYUDP_PORT);
	
	// Copy the IP address 
	memcpy(&(ser_addr.sin_addr.s_addr), *addrs, sizeof(struct in_addr));
	
	// Zero out the sin_zero array
	bzero(&(ser_addr.sin_zero), 8);
	
	// Read data from local file
	if((fp = fopen ("myfile.txt","r+t")) == NULL)		
	{
		printf("File doesn't exit\n");
		exit(0);
	}
	
	// Transmit data to server and calculate transmission rate
	ti = str_cli(fp, sockfd, (struct sockaddr *)&ser_addr, sizeof(struct sockaddr_in), &len);
	rt = (len/(float)ti);
	printf("Time(ms) : %.3f, Data sent(byte): %d\nData rate: %f (Kbytes/s)\n", ti, (int)len, rt);

	/*
	// Consecutive message send for data collection
	float totalTime = 0.0, totalRate = 0.0, avgTime, avgRate;
	int iterations = 10;
	for(int i = 0; i < iterations; i++){
		ti = str_cli(fp, sockfd, (struct sockaddr *)&ser_addr, sizeof(struct sockaddr_in), &len);
		rt = (len/(float)ti);
		totalTime += ti;
		totalRate += rt;
		printf("Time(ms) : %.3f, Data rate: %f (Kbytes/s)\n", ti, rt);
	}
	avgTime = totalTime / iterations;
	avgRate = totalRate / iterations;
	printf("\n[%d Iterations] Average Time(ms) : %.3f, Average Rate: %f (Kbytes/s)\n", iterations, avgTime, avgRate);
	*/
	
	// Exit
	fclose(fp);
	close(sockfd);
	exit(0);
}

float str_cli(FILE *fp, int sockfd, struct sockaddr *addr, int addrlen, long *len)
{
	char *buf, sends[DATALEN];
	long lsize, ci;
	struct ack_so ack;
	struct timeval sendt, recvt;
	int n, slen, duMultiplier, mlt;
	float time_inv = 0.0;
	bool sendDouble;
	
	// Set current index to zero
	ci = 0;

	fseek (fp , 0 , SEEK_END); // Set position to EOF
	lsize = ftell (fp); // Get file size from current position to start of file
	rewind (fp); // Reset position to start of file
	printf("File length: %d bytes\n", (int)lsize);
	printf("Packet length: %d bytes\n", DATALEN);

	// Allocate memory to contain the whole file.
	buf = (char *) malloc (lsize);
	if (buf == NULL) exit (2);

	// Copy the file into the buffer.
	fread (buf,1,lsize,fp);

	// Load the whole file into the buffer
	buf[lsize] ='\0'; //append the end byte
	gettimeofday(&sendt, NULL);	//get the current time
	
	// Set our DU size to alternate between 1 and 2 if not in S&W mode
	duMultiplier = STOPANDWAITFLAG ? 1 : 2;
	
	// Start with sending 1 DU
	sendDouble = false;
	
	/** Start of transmission loop **/
	while(ci <= lsize)
	{
		// Send duMultiplier number of packets
		for(int i = 0; i < duMultiplier; i++){
			if ((lsize+1-ci) <= DATALEN){
				slen = lsize+1-ci;
			} else { 
				slen = DATALEN;
			}
			// Copy the data to be sent
			memcpy(sends, (buf+ci), slen);
			
			// Send data to specified server addr, return number of char sent or error code 
			n = sendto(sockfd, &sends, slen, 0, addr, addrlen);
			/*
			 send(): TCP SOCK_STREAM connected sockets
			 sendto(): UDP SOCK_DGRAM unconnected datagram sockets
			 Destination of a packet must be specified each time we send a packet over unconnected socket
			*/
			
			// Exit program if there is an error in sending
			if(n == -1) {
				printf("Send error!");
				exit(1);
			} 
			
			// If send success, advance the current index by the length of data sent
			ci += slen;
			
			// Break out of the for loop if we're only sending 1 DU
			if(!sendDouble){
				break;
			}
		}

		// Catch ACK errors
		if ((n= recv(sockfd, &ack, 2, 0))==-1) {
			printf("Error when receiving ack\n");
			exit(1);
		}
		if (ack.num != 1|| ack.len != 0) {
			printf("Error in transmission\n");
		}
		// Debug ACK feedback
		// printf("Acknowledged up to [%ld]\n", ci);
		
		// Invert the boolean variable for sending 1 DU / 2 DU
		sendDouble = !sendDouble;
	}
	/** End of transmission loop **/
	
	gettimeofday(&recvt, NULL);
	*len= ci; //get current time
	tv_sub(&recvt, &sendt); // get the whole trans time
	time_inv += (recvt.tv_sec)*1000.0 + (recvt.tv_usec)/1000.0;
	return(time_inv);
}

void tv_sub(struct timeval *out, struct timeval *in) {
  if ((out->tv_usec -= in->tv_usec) < 0) {
    --out->tv_sec;
    out->tv_usec += 1000000;
  }
  out->tv_sec -= in->tv_sec;
}

// https://stackoverflow.com/questions/791982/determine-if-a-string-is-a-valid-ipv4-address-in-c
bool validIP(char *ipAddr)
{
    struct sockaddr_in sa;
    int result = inet_pton(AF_INET, ipAddr, &(sa.sin_addr));
    return result != 0;
}