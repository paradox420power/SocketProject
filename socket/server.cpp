//compiled as "g++ -o server -pthread server.cpp"
#include <sys/socket.h> 	/* for socket() and bind() */
#include <stdio.h>		/* printf() and fprintf() */
#include <stdlib.h>		/* for atoi() and exit() */
#include <arpa/inet.h>	/* for sockaddr_in and inet_ntoa() */
#include <sys/types.h>
#include <string.h>
#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

#define	MES_MAX	255		/* Longest string to echo */
#define BACKLOG	128

using namespace std;

struct player{
	string name;
	string IP;
	string port;
};
static vector<player> playerList; //vector to track all registered players
pthread_mutex_t pListUpdate = PTHREAD_MUTEX_INITIALIZER; //only 1 thread should be modifying the List at a time

struct ThreadArgs{
	int sockfd;
};

int getPlayerIndex(string pName){
	int index = -1;
	for(int x = 0; x < playerList.size(); x++){
		if(playerList[x].name.compare(pName) == 0)
			index = x;
	}
	return index;
}

void DieWithError(const char *errorMessage){//included in sample code, carried over to project
	perror(errorMessage);
	exit(1);
}

int registerPlayer(int sockfd){
	ssize_t n;
    char    line[MES_MAX];
	string command = ""; //Register, Query, Start, Query, End, Deregister
	string name = ""; //<Player Name>, Players, Game, Games (determined by command)
	string IP = ""; //IP Addr., Player Count, Game ID (determined by command)
	string port = ""; //only used in register command
	bool unreg = true;
	bool stillConnected = true;
	int isReg = 0;
	
	while(unreg && stillConnected){
		if ( (n = read(sockfd, line, MES_MAX)) == 0 )
			stillConnected = false; //client immediately disconnects
		
		string message = string(line); //convert char message into string
		int count = 0;
		istringstream iss(message); //delimit spaces from the message
		do{
			string part;
			iss >> part;
			switch(count){
				case 0: command = part;
					break;
				case 1: name = part;
					break;
				case 2: IP = part;
					break;
				case 3: port = part;
				default: //do nothing
					break;
			}
			count++;
		}while(iss);
		//interpret the command
		if(command.compare("register") == 0 || command.compare("Register") == 0){ //Register Player IP
			//add player to list
			if(name.compare("") != 0 && IP.compare("") != 0 && port.compare("") != 0){ //ensure all data was passed to server
				if(getPlayerIndex(name) == -1){ //don't allow duplicate player names
					playerList.push_back(player());
					playerList[playerList.size()-1].name = name;
					playerList[playerList.size()-1].IP = IP;
					playerList[playerList.size()-1].port = port;
					write(sockfd, "0", 1 ); //return success
					unreg = false;
					isReg = 1;
				}else{
					write(sockfd, "1", 1 ); //return failure
				}
			}else{
				write(sockfd, "1", 18); //return failure
			}
			
		}
	}
	return isReg; //0 if they disconnect, 1 if they register correctly	
}

void *serverInterface(void *threadArgs){
    
	int sockfd;
	pthread_detach(pthread_self());
	sockfd = ((struct ThreadArgs *) threadArgs ) -> sockfd;
	free(threadArgs);
	ssize_t n;
    char    line[MES_MAX];
	string command = ""; //Register, Query, Start, Query, End, Deregister
	string value = ""; //<Player Name>, Players, Game, Games (determined by command)
	string numeric = ""; //IP Addr., Player Count, Game ID (determined by command)
	string port = ""; //only used in register command
	bool validInput = false;
	
	printf("Handling %d\n", sockfd); //debug info
	bool stillConnected = true;
	if ( (n = read(sockfd, line, MES_MAX)) == 0 )
   	    stillConnected = false; //client immediately disconnects
	
	while(stillConnected){
		
		string message = string(line); //convert char message into string
		command = ""; //reset fields
		value = "";
		numeric = "";
		port = "";
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
				case 3: port = part;
				default: //do nothing
					break;
			}
			count++;
		}while(iss);
		
		cout << command << " " << value << " " << numeric << " " << port << endl;
		
		//interpret the command
		validInput = false;
		//if(command.compare("register") == 0 || command.compare("Register") == 0){ //everyone should register before entering this function
		if(command.compare("query") == 0 || command.compare("Query") == 0){ //Query Players or Games
			
			if(value.compare("players") == 0 || value.compare("Players") == 0){
				stringstream ss;
				ss << playerList.size();
				string playerCount = ss.str() + " ";
				write(sockfd, playerCount.c_str(), playerCount.size());
				
				for(int x = 0; x < playerList.size(); x++){
					string toSend = playerList[x].name + " " + playerList[x].IP + " " + playerList[x].port + " "; //end with space to ensure delimiting
					write(sockfd, toSend.c_str(), toSend.size());
				}
				validInput = true;
				
			}else if(value.compare("games") == 0 || value.compare("Games") == 0){
				write(sockfd, "ask games", 9 );
				validInput = true;
			}
			
		}else if(command.compare("start") == 0 || command.compare("Start") == 0){ //Start Game k
			if(value.compare("game") == 0 || value.compare("Game") == 0){
				int playerCount;
				istringstream buffer(numeric);
				iss >> playerCount;
				//get int & start game
				write(sockfd, "Start Game", 11 );
				validInput = true;
			}
			
		}else if(command.compare("end") == 0 || command.compare("End") == 0){ //End <Game ID>
			//search & remove ID from list
			write(sockfd, "End Game", 8 );
			validInput = true;
			
		}else if(command.compare("deregister") == 0 || command.compare("Deregister") == 0){ //Deregister Player
			//search & remove player from list
			pthread_mutex_lock(&pListUpdate); //don't execute if someon else is (de)registering
			//TODO
			pthread_mutex_unlock(&pListUpdate);
			write(sockfd, "Drop Player", 11 );
			validInput = true;
		}
		
		if(!validInput){
			write(sockfd, "Input not recognized", 31 );
			validInput = true;
		}
		
		cout << "End process" << endl;
		memset(line, 0, 255); //clean out the buffer before recievving next input
		if ( (n = read(sockfd, line, MES_MAX)) == 0 ) //read next command
   	    	stillConnected = false; /* connection closed by other end */
	}
	//return; //no return value
	
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
	
	int isReg;
	pthread_t threadID;
	struct ThreadArgs *threadArgs;
	
	for(;;){ //loop the server indefinitely
		cliAddrLen = sizeof(echoClntAddr);
		connfd = accept( sock, (struct sockaddr *) &echoClntAddr, &cliAddrLen );
		
		pthread_mutex_lock(&pListUpdate); //lock player list before modifying it
		isReg = registerPlayer(connfd); //register before forking off
		pthread_mutex_unlock(&pListUpdate); //unlock for further editing
		
		if(isReg == 1){
			
			printf("Handling client %s\n", inet_ntoa(echoClntAddr.sin_addr));
			threadArgs = (struct ThreadArgs *)malloc(sizeof(struct ThreadArgs));
			threadArgs -> sockfd = connfd;
			pthread_create(&threadID, NULL, serverInterface, (void *) threadArgs);
			
		}else{ //they disconnected very early
			close(connfd);
			cout << "Client failed to Regsiter" << endl;
		}
	}
}
