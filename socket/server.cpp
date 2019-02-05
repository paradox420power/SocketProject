#include <sys/socket.h> 	/* for socket() and bind() */
#include <stdio.h>		/* printf() and fprintf() */
#include <stdlib.h>		/* for atoi() and exit() */
#include <arpa/inet.h>	/* for sockaddr_in and inet_ntoa() */
#include <sys/types.h>
#include <string.h>
#include <string>
#include <iostream>
#include <sstream>
#include <unistd.h>
#include <pthread.h>

#define	MES_MAX	255		/* Longest string to echo */
#define BACKLOG	128

using namespace std;

void DieWithError(const char *errorMessage){//included in sample code, carried over to project
	perror(errorMessage);
	exit(1);
}

void serverInterface(int sockfd){
    ssize_t n;
    char    line[MES_MAX];
	string command = ""; //Register, Query, Start, Query, End, Deregister
	string value = ""; //<Player Name>, Players, Game, Games (determined by command)
	string numeric = ""; //IP Addr., Player Count, Game ID (determined by command)
	bool validInput = false;
	
	printf("Handling %d\n", sockfd); //debug info
	bool stillConnected = true;
	if ( (n = read(sockfd, line, MES_MAX)) == 0 )
   	    stillConnected = false; //client immediately disconnects
	
	while(stillConnected){
		
		string message = string(line); //convert char message into string
		string command = ""; 
		string value = "";
		string numeric = "";
		int count = 0;
		istringstream iss(message); //delimit spaces from the message
		do{
			string part;
			iss >> part;
			switch(count){
				case 0: command = part;
					break;
				case 1: value = part;
					break;
				case 2: numeric = part;
					break;
				default: //do nothing
					break;
			}
			count++;
		}while(iss);
		
		cout << command << " " << value << " " << numeric << endl;
		
		//interpret the command
		validInput = false;
		if(command.compare("register") == 0 || command.compare("Register") == 0){ //Register Player IP
			//add player to list
			write(sockfd, "Player Added\n", 13 );
			validInput = true;
		}else if(command.compare("query") == 0 || command.compare("Query") == 0){ //Query Players or Games
			if(value.compare("players") == 0 || value.compare("Players") == 0){
				write(sockfd, "ask player\n", 11 );
				validInput = true;
			}else if(value.compare("games") == 0 || value.compare("Games") == 0){
				write(sockfd, "ask games\n", 10 );
				validInput = true;
			}
		}else if(command.compare("start") == 0 || command.compare("Start") == 0){ //Start Game k
			if(value.compare("game") == 0 || value.compare("Game") == 0){
				int playerCount;
				istringstream buffer(numeric);
				iss >> playerCount;
				//get int & start game
				write(sockfd, "Start Game\n", 12 );
				validInput = true;
			}
		}else if(command.compare("end") == 0 || command.compare("End") == 0){ //End <Game ID>
			//search & remove ID from list
			write(sockfd, "End Game\n", 9 );
			validInput = true;
		}else if(command.compare("deregister") == 0 || command.compare("Deregister") == 0){ //Deregister Player
			//search & remove player from list
			write(sockfd, "Drop Player\n", 12 );
			validInput = true;
		}
		
		if(!validInput){
			write(sockfd, "Input not recognized\n", 32 );
			validInput = true;
		}
			
		if ( (n = read(sockfd, line, MES_MAX)) == 0 ) //read next command
   	    	stillConnected = false; /* connection closed by other end */
	}
	return; //no return value
	
}


int main(int argc, char **argv){
    int sock, connfd;                /* Socket */
    struct sockaddr_in echoServAddr; /* Local address */
    struct sockaddr_in echoClntAddr; /* Client address */
    unsigned int cliAddrLen;         /* Length of incoming message */
    char echoBuffer[MES_MAX];        /* Buffer for echo string */
    unsigned short echoServPort;     /* Server port */
    int recvMsgSize;                 /* Size of received message */

    echoServPort = 36000;  //local port assigned for group number 35

    /* Create socket for sending/receiving datagrams */
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        DieWithError("server: socket() failed");

    /* Construct local address structure */
    memset(&echoServAddr, 0, sizeof(echoServAddr));   /* Zero out structure */
    echoServAddr.sin_family = AF_INET;                /* Internet address family */
    echoServAddr.sin_addr.s_addr = htonl(INADDR_ANY); /* Any incoming interface */
    echoServAddr.sin_port = htons(echoServPort);      /* Local port */
	
	
    /* Bind to the local address */
    if (bind(sock, (struct sockaddr *) &echoServAddr, sizeof(echoServAddr)) < 0)
        DieWithError("server: bind() failed");
  
	if (listen(sock, BACKLOG) < 0 )
		DieWithError("server: listen() failed");
		
	for(;;){ //loop the server indefinitely
		cliAddrLen = sizeof(echoClntAddr);
		connfd = accept( sock, (struct sockaddr *) &echoClntAddr, &cliAddrLen );
		
		int processID = fork();
		if(processID == 0){ //create a child thread for new client
			printf("Handling client %s\n", inet_ntoa(echoClntAddr.sin_addr));
			serverInterface(connfd);
			printf("Connection Closed\n");
			close(connfd);
		}
	}
}
