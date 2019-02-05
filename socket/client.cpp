#include <sys/socket.h>  /* for socket() and bind() */
#include <stdio.h>               /* printf() and fprintf() */
#include <stdlib.h>              /* for atoi() and exit() */
#include <arpa/inet.h>   /* for sockaddr_in and inet_ntoa() */
#include <sys/types.h>
#include <string.h>
#include <string>
#include <unistd.h>
#include <iostream>

#define ECHOMAX 255             /* Longest string to echo */
#define BACKLOG 128

using namespace std;

struct message{
	string command; //Register, Query, Start, Query, End, Deregister
	string value; //<Player Name>, Players, Game, Games (determined by command)
	string numeric; //IP Addr., Player Count, Game ID (determined by command)
};

void DieWithError(const char *errorMessage){ //included in sample code, carried over to project
        perror(errorMessage);
        exit(1);
}

void clientInput(FILE *fp, int sockfd){
	ssize_t n;
    char    sendline[ECHOMAX], recvline[ECHOMAX];
	
	while (fgets(sendline, ECHOMAX, fp) != NULL) {
			
        write(sockfd, sendline, strlen(sendline));

        if ( (n = read(sockfd, recvline, ECHOMAX)) == 0)
            DieWithError("str_cli: server terminated prematurely");

		recvline[ n ] = '\0';
        fputs(recvline, stdout);
    }
}

int main(int argc, char **argv){
	int sockfd;
	struct sockaddr_in servaddr;

	if (argc != 2) //don't need port, only IP
		DieWithError( "usage: tcp-client <Server-IPaddress> <Server-Port>" );
	
	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(36000); //hardcoded handshake port
	inet_pton(AF_INET, argv[1], &servaddr.sin_addr);

	connect(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr));

	clientInput(stdin, sockfd); //client is created, now can interact

	exit(0);
}
