/* udpServer.c: Server to receive 1 DU, send ACK, receive 2 DU, send ACK, repeat
 *
 * Flag to switch between alternating and S&W mode found in headsock.h
 *
 * NG WAI HUNG A0155269W
*/

#include "headsock.h"
void str_ser(int sockfd, struct sockaddr *addr, int addrlen);   

int main(void) {
	int sockfd, ret;
	struct sockaddr_in my_addr;

	// Create socket
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1){
		printf("Error in socket!");
		exit(1);
	}

	// Match the address family to IPv4 format
	my_addr.sin_family = AF_INET;
	
	// Specify the port number in big endian format
	my_addr.sin_port = htons(MYUDP_PORT);
	my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	
	// Zero out the sin_zero array
	bzero(&(my_addr.sin_zero), 8);

	// Bind to available UDP port
	if (bind(sockfd, (struct sockaddr *) &my_addr, sizeof(struct sockaddr)) == -1) {
		printf("Error in binding");
		exit(1);
	}

	printf("Waiting for data\n");
	
	// Receiving Loop
	while(1){
		str_ser(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr_in));
	}
	
	close(sockfd);
	exit(0);
}

void str_ser(int sockfd, struct sockaddr *addr, int addrlen) {
	char buf[BUFSIZE];
	FILE *fp;
	char recvs[DATALEN];
	struct ack_so ack;
	int end = 0, n = 0, status = 0;
	long lseek = 0;

	printf("Receiving data...\n");

	while (!end) {
		// printf("Status: %d\n", status);
		
		// Receive packet
		if ((n= recvfrom(sockfd, &recvs, DATALEN, 0, addr, (socklen_t *)&addrlen))==-1) {
			printf("Error when receiving\n");
			exit(1);
		}

		// Terminating condition
		if (recvs[n - 1] == '\0') {
			end = 1;
			n--;
		}

		memcpy((buf + lseek), recvs, n);
		lseek += n;
		
		// If status is 0 (received 1st DU) or 2 (received 3rd DU)
		if(STOPANDWAITFLAG || status  == 0 || status == 2){
			ack.num = 1;
			ack.len = 0;
			if ((n = sendto(sockfd, &ack, 2, 0, addr, addrlen)) == -1) {
				printf("Send error!");
				exit(1);
			} 
			// printf("ACK sent!\n");
		}
		
		// Increment status, reset to 0 once the 1DU + 2DU cycle is completed
		status = (status == 2) ? 0 : status + 1;
	}

	if ((fp = fopen("myUDPreceive.txt", "wt")) == NULL) {
		printf("File doesn't exit\n");
		exit(0);
	}
	fwrite(buf, 1, lseek, fp);
	fclose(fp);
	printf("A file has been successfully received!\nthe total data received is %d bytes\n", (int) lseek);
}
