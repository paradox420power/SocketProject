#include <sys/socket.h>  /* for socket() and bind() */
#include <stdio.h>               /* printf() and fprintf() */
#include <stdlib.h>              /* for atoi() and exit() */
#include <arpa/inet.h>   /* for sockaddr_in and inet_ntoa() */
#include <sys/types.h>
#include <string.h>
#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <unistd.h>
#include <iostream>
#include <pthread.h>
#include <semaphore.h>

#define ECHOMAX 255             /* Longest string to echo */
#define BACKLOG 128

using namespace std;

string registerPlayer(string IP, int port, int sockfd);
void *clientInput(void *threadArg);
void *serverOutput(void *threadArg);
void *P2Phost(void *P2Parg);
void *P2Pplayer(void *P2Parg);

struct ThreadArgs{
	int port;
	int sockfd;
};
pthread_mutex_t changeCommands = PTHREAD_MUTEX_INITIALIZER; //lock commands vector from having things added & removed at same time
char sendline[ECHOMAX], recvline[ECHOMAX], playLine[ECHOMAX];

struct P2Pargs{
	char IP[ECHOMAX];
	char port[ECHOMAX];
	int pCount;
};

void DieWithError(const char *errorMessage){ //included in sample code, carried over to project
        perror(errorMessage);
        exit(1);
}

//registering is a back & forth until success so it is better to have a method of input & output
string registerPlayer(string IP, int port, int sockfd){
	ssize_t n;
	string pickedName = "";
	bool success = false;
	string input, command, name, part;
	int count = 0;
	cout << "Please Register: (format Register <player name>" << endl;
	while(success == false && fgets(sendline, ECHOMAX, stdin) != NULL){
		input = string(sendline);
		command = ""; //reset fields to be sure
		name = "";
		count = 0;
		istringstream buf(input);
		do{
			buf >> part;
			switch(count){
				case 0: command = part;
					break;
				case 1: name = part;
					break;
				default: //do nothing
					break;
			}
			count++;
		}while(buf);
		
		if(command.compare("register") == 0 || command.compare("Register") == 0){ //Register Player IP
			//add player to list
			if(name.compare("") != 0){ //make sure name was passed
				stringstream ss;
				ss << port;
				input = command + " " + name + " 0.0.0.0 " + ss.str() + " ";
				write(sockfd, input.c_str(), input.size());
				memset(recvline, 0 ,255);
				if ( (n = read(sockfd, recvline, ECHOMAX) == 0) )
					DieWithError("str_cli: server terminated prematurely");
				string output = string(recvline);
				istringstream buf(output);
				int successField;
				buf >> successField;
				if(successField == 0){
					cout << "Registered" << endl;
					pickedName = name;
					success = true;
				}else{
					cout << "Failed to Register" << endl;
				}
			}
		}else{
			cout << "Incorrect command issued" << endl;
		}
	}
	
	if(success){ //only spawn output thread if client registered
		pthread_t outThread; //now that they are registered we should split off a server output thread
		struct ThreadArgs *outputArgs;
		outputArgs = (struct ThreadArgs *)malloc(sizeof(struct ThreadArgs));
		outputArgs -> sockfd = sockfd;
		pthread_create(&outThread, NULL, serverOutput, (void *) outputArgs); //should be passed same sockfd as client input
	}
	return pickedName;
}

bool stillConnected = true;

void *clientInput(void *threadArg){
	int sockfd, port, count;
	string myName, message, IP, command, value, part;
	pthread_detach(pthread_self());
	sockfd = ((struct ThreadArgs *) threadArg ) -> sockfd;
	port = ((struct ThreadArgs *) threadArg ) -> port;
	myName = registerPlayer(IP, port, sockfd); //cal this here
	free(threadArg);
	
	if(myName.compare("") != 0){ //actually registered a name
		while(stillConnected && fgets(sendline, ECHOMAX, stdin) != NULL){
			message = string(sendline);
			command = "";
			value = "";
			count = 0;
			istringstream buf(message);
			do{
				buf >> part;
				switch(count){
					case 0: command = part;
						break;
					case 1: value = part;
						break;
					default: //do nothing
						break;
				}
				count++;
			}while(buf);
			if(command.compare("deregister") == 0 || command.compare("Deregister") == 0){
				if(value.compare(myName) == 0){
					write(sockfd, sendline, strlen(sendline));
					stillConnected = false;
				}else{
					cout << "You may only deregister yourself" << endl;
				}
			}else{
				write(sockfd, sendline, strlen(sendline));
			}
		}
	} //otherwise the user disconnected before registering
	if(stillConnected){ //if the user exits prematurely the client must auto-deregister
		string autoDereg = "Deregister " + myName;
		write(sockfd, autoDereg.c_str(), autoDereg.size());
		stillConnected = false;
	}
}

vector<string> toPrint; //this will be the vector of everything the server is sending to the client

void readInput(int sockfd){ //this get calls a bit by the server, make it it's own method
	ssize_t n;
	string message, part;
	memset(recvline, 0, 255); //clean buffer before receiving next
	if ( (n = read(sockfd, recvline, ECHOMAX) == 0) )
		DieWithError("str_cli: server terminated prematurely");
	message = string(recvline);
	istringstream buffer(message);
	while(getline(buffer, part, ' ')){ //read everything from recvline & push back delimited by spaces
		toPrint.push_back(part);
	}
}

void *serverOutput(void *threadArg){
	ssize_t n;
	int sockfd;
	pthread_detach(pthread_self());
	sockfd = ((struct ThreadArgs *) threadArg ) -> sockfd;
	free(threadArg);
	string nextCommand, message, part;
	
	while(stillConnected || toPrint.size() > 0){ //keep looping while client is passing commands or the client has output to handle
		readInput(sockfd);
		
		//interpret command
		nextCommand = toPrint[0];
		toPrint.erase(toPrint.begin());
		if(nextCommand.compare("Query") == 0 || nextCommand.compare("query") == 0){
			nextCommand = toPrint[0];
			toPrint.erase(toPrint.begin());
			if(nextCommand.compare("players") == 0 || nextCommand.compare("Players") == 0){
				if(toPrint.size() == 0){ //need to ensure we have the player count in toPrint before continuing
					readInput(sockfd);
				}
				int playerCount;
				istringstream convert(toPrint[0]); //first value will be palyer count
				convert >> playerCount;
				toPrint.erase(toPrint.begin()); //remove head
				int readPlayers = 0;
				while(readPlayers < playerCount){
					if(toPrint.size() == 0){ //we've read everything in toPrint, but not all palyers have been read in yet
						readInput(sockfd);
					}
					cout << "Player " << toPrint[0] << " at IP " << toPrint[1] << " and port " << toPrint[2] << endl; //each player triple should be passed all together
					readPlayers++; //increment the player count
					toPrint.erase(toPrint.begin(), toPrint.begin()+3); //erase the read palyer
				}
				
				
			}else{ //should be games query
				cout << "Query Games" << endl;
			}
			
		}else if(nextCommand.compare("start") == 0 || nextCommand.compare("Start") == 0){
			if(toPrint.size() == 0){
				readInput(sockfd);
			}
			nextCommand = toPrint[0]; //either host or player
			toPrint.erase(toPrint.begin());
			pthread_t threadID;
			if(nextCommand.compare("Host") == 0){
				//spawn host thread
				string playCount = toPrint[0];
				toPrint.erase(toPrint.begin());
				string toConnect = toPrint[0];
				toPrint.erase(toPrint.begin());
				string portToUse = toPrint[0];
				toPrint.erase(toPrint.begin());
				cout << "You are the host at " << toConnect << " & port " << portToUse << endl;
				struct P2Pargs * gameDet;
				gameDet = (struct P2Pargs *)malloc(sizeof(P2Pargs));
				stpcpy(gameDet->IP, toConnect.c_str()); //passed args must be in char array
				strcpy(gameDet->port, portToUse.c_str());
				istringstream buffer(playCount);
				buffer >> gameDet->pCount;
				pthread_create(&threadID, NULL, P2Phost, (void *) gameDet);
				
			}else if(nextCommand.compare("Player") == 0){
				//spawn player thread relative to that a host
				string toConnect = toPrint[0];
				toPrint.erase(toPrint.begin());
				string portToUse = toPrint[0];
				toPrint.erase(toPrint.begin());
				cout << "You are the player at " << toConnect << " & port " << portToUse << endl;
				struct P2Pargs * gameDet;
				gameDet = (struct P2Pargs *)malloc(sizeof(P2Pargs));
				stpcpy(gameDet->IP, toConnect.c_str()); //passed args must be in char array
				strcpy(gameDet->port, portToUse.c_str());
				pthread_create(&threadID, NULL, P2Pplayer, (void *) gameDet);
				
			}else{ //error returned
				cout << nextCommand << endl;
			}
			
		}else if(nextCommand.compare("end") == 0 || nextCommand.compare("End") == 0){
			cout << "End" << endl;
			
		}else if(nextCommand.compare("deregister") == 0 || nextCommand.compare("Deregister") == 0){
			if(toPrint.size() == 0){
				readInput(sockfd);
			}
			message = toPrint[0];
			toPrint.erase(toPrint.begin());
			cout << message << endl;
			
		}else{ //should be an error code
			cout << "Possible Error" << endl;
			
		}
			
	}
}

void *P2Phost(void *P2Parg){ //thread spawned for BINGO hosts
	string IP;
	int pCount;
	unsigned short hostPort;
	pthread_detach(pthread_self());
	IP = string(((struct P2Pargs *) P2Parg ) -> IP); //convert args back to string
	hostPort = (unsigned short)strtoul(((struct P2Pargs *) P2Parg ) -> port, NULL ,0);
	pCount = ((struct P2Pargs *) P2Parg ) -> pCount;
	free(P2Parg);
	
	int sock, connfd;                /* Socket */
    struct sockaddr_in hostAddr; /* Local address */
    struct sockaddr_in playAddr; /* Client address */
    unsigned int cliAddrLen;         /* Length of incoming message */
	
	cout << "A" << endl;
	
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        DieWithError("server: socket() failed");

    /* Construct local address structure */
    memset(&hostAddr, 0, sizeof(hostAddr));   /* Zero out structure */
    hostAddr.sin_family = AF_INET;                /* Internet address family */
    hostAddr.sin_addr.s_addr = htonl(INADDR_ANY); /* Any incoming interface */
    hostAddr.sin_port = htons(hostPort);      /* Local port */
	
	cout << "B" << endl;
	
	if (bind(sock, (struct sockaddr *) &hostAddr, sizeof(hostAddr)) < 0)
        DieWithError("server: bind() failed");
	
	cout << "C" << endl;
	
	if (listen(sock, BACKLOG) < 0 )
		DieWithError("server: listen() failed");
	
	cout << "D" << endl;
	
	for(int x = 0; x < pCount; x++){
		cliAddrLen = sizeof(playAddr);
		connfd = accept( sock, (struct sockaddr *) &playAddr, &cliAddrLen );
		
		cout << "E" << endl;
		
		printf("Handling client %s\n", inet_ntoa(playAddr.sin_addr));
		string test = "TEST";
		write(connfd, test.c_str(), test.size());
	}
}


void *P2Pplayer(void *P2Parg){ //thread spawned for BINGO players
	char IP[ECHOMAX];
	unsigned short hostPort;
	pthread_detach(pthread_self());
	strcpy(IP, ((struct P2Pargs *) P2Parg ) -> IP); //convert args back to string
	hostPort = (unsigned short)strtoul(((struct P2Pargs *) P2Parg ) -> port, NULL ,0);
	free(P2Parg);
	
	int sockfd;
	struct sockaddr_in hostAddr;
	
	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	bzero(&hostAddr, sizeof(hostAddr));
	hostAddr.sin_family = AF_INET;
	hostAddr.sin_port = htons(hostPort); //passed handshake port
	inet_pton(AF_INET, IP, &hostAddr.sin_addr);
	
	connect(sockfd, (struct sockaddr *) &hostAddr, sizeof(hostAddr));
	string test;
	ssize_t n;
	memset(playLine, 0, 255); //clean buffer before receiving next
	if ( (n = read(sockfd, playLine, ECHOMAX) == 0) )
		DieWithError("str_cli: server terminated prematurely");
	test = string(playLine);
	
	cout << test << endl;
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

	
	pthread_t inThread;
	struct ThreadArgs *inputArgs;
	inputArgs = (struct ThreadArgs *)malloc(sizeof(struct ThreadArgs));
	inputArgs -> sockfd = sockfd;
	inputArgs -> port = servaddr.sin_port;
	
	
	pthread_create(&inThread, NULL, clientInput, (void *) inputArgs);
	pthread_exit(NULL); //don't end program because this thread ended

	exit(0);
}
