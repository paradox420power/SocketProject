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
	int sockfd;
};
static vector<player> playerList; //vector to track all registered players
pthread_mutex_t pListUpdate = PTHREAD_MUTEX_INITIALIZER; //only 1 thread should be modifying the List at a time
bool freePorts[1000];

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

int getPlayerIndex(int pSockfd){
	int index = -1;
	for(int x = 0; x < playerList.size(); x++){
		if(playerList[x].sockfd == pSockfd)
			index = x;
	}
	return index;
}

int getNextFreePort(){
	int port = -1;
	for(int x = 0; x < 1000; x++){
		if(freePorts[x] == true){
			port = x;
			break;
		}
	}
	return port;
}

void DieWithError(const char *errorMessage){//included in sample code, carried over to project
	perror(errorMessage);
	exit(1);
}


string registerPlayer(int sockfd, string passIP){
	ssize_t n;
    char    line[MES_MAX];
	string command = ""; //Register, Query, Start, Query, End, Deregister
	string name = ""; //<Player Name>, Players, Game, Games (determined by command)
	string IP = passIP; //IP Addr., Player Count, Game ID (determined by command)
	string port = ""; //only used in register command
	bool unreg = true;
	bool stillConnected = true;
	string pickedName = "";
	
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
				case 2: IP = passIP; //use the IP the server received connection from
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
					playerList[playerList.size()-1].sockfd = sockfd;
					write(sockfd, "0", 1 ); //return success
					unreg = false;
					pickedName = name;
				}else{
					write(sockfd, "1", 1 ); //return failure
				}
			}else{
				write(sockfd, "1", 18); //return failure
			}
			
		}
	}
	return pickedName; //empty if they disconnect, the name if they register correctly	
}

void spawnP2P(int currentSock, int playerReq, string IP, string port){
	bool chosen[playerList.size()];
	int pCount = 0;
	int randPick;
	string mes = "Start Player " + IP + " " + port + " "; //tell them they are a player, on this IP, at this port #
	while(pCount < playerReq){
		randPick = rand() % playerList.size(); //get random number within bounds of array
		if(chosen[randPick] == false && playerList[randPick].sockfd != currentSock){ //get random other player, not host
			write(playerList[randPick].sockfd, mes.c_str(), mes.size()); //notify them
			chosen[randPick] = true;
			pCount++;
		}
	}
}

void *serverInterface(void *threadArgs){
    
	int sockfd;
	string clientName;
	pthread_detach(pthread_self());
	sockfd = ((struct ThreadArgs *) threadArgs ) -> sockfd;
	free(threadArgs);
	int index = getPlayerIndex(sockfd);
	clientName = playerList[index].name;
	ssize_t n;
    char    line[MES_MAX];
	string command = ""; //Register, Query, Start, Query, End, Deregister
	string value = ""; //<Player Name>, Players, Game, Games (determined by command)
	string numeric = ""; //IP Addr., Player Count, Game ID (determined by command)
	string port = ""; //only used in register command
	string sendCom;
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
				sendCom = command + " " + value + " ";
				write(sockfd, sendCom.c_str(), sendCom.size());
				
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
				sendCom = command + " " + value + " ";
				write(sockfd, sendCom.c_str(), sendCom.size());
				write(sockfd, "ask", 3 );
				validInput = true;
			}
			
		}else if(command.compare("start") == 0 || command.compare("Start") == 0){ //Start Game k
			if(value.compare("game") == 0 || value.compare("Game") == 0){
				sendCom = command + " ";
				write(sockfd, sendCom.c_str(), sendCom.size());
				int playerCount;
				istringstream buffer(numeric);
				buffer >> playerCount;
				//get int & start game
				if(playerCount < playerList.size()){
					int index = getPlayerIndex(sockfd);
					string toConnect = playerList[index].IP;
					int portToUse = getNextFreePort() + 36000; //add 36000 since my port range is [36000, 36999]
					stringstream port;
					port << portToUse;
					if(portToUse != -1){
						sendCom = "Host " + numeric + " " + toConnect + " " + port.str() + " "; //numeric is player count
						write(sockfd, sendCom.c_str(), sendCom.size() );
						spawnP2P(sockfd, playerCount, toConnect, port.str());
					}else{ //all ports in use
						sendCom = "NO_PORTS ";
						write(sockfd, sendCom.c_str(), sendCom.size() );
					}
				}else
					write(sockfd, "Insufficient", 12);
				validInput = true;
			}
			
		}else if(command.compare("end") == 0 || command.compare("End") == 0){ //End <Game ID>
			//search & remove ID from list
			sendCom = command + " ";
			write(sockfd, sendCom.c_str(), sendCom.size());
			write(sockfd, "End", 3 );
			validInput = true;
			
		}else if(command.compare("deregister") == 0 || command.compare("Deregister") == 0){ //Deregister Player
			//search & remove player from list
			sendCom = command + " ";
			write(sockfd, sendCom.c_str(), sendCom.size());
			pthread_mutex_lock(&pListUpdate); //don't execute if someon else is (de)registering
			//TODO
			index = getPlayerIndex(value); //each player has unique sockfd
			if(index != -1)
				playerList.erase(playerList.begin() + index);
			pthread_mutex_unlock(&pListUpdate);
			write(sockfd, "Drop", 4 );
			validInput = true;
		}
		
		if(!validInput){
			write(sockfd, "Error", 5 );
			validInput = true;
		}
		
		cout << "End process" << endl;
		memset(line, 0, 255); //clean out the buffer before recievving next input
		if ( (n = read(sockfd, line, MES_MAX)) == 0 ) //read next command
   	    	stillConnected = false; /* connection closed by other end */
	}
	if(playerList[getPlayerIndex(sockfd)].name.compare(clientName) == 0){//disconnected prematurely, remove from player list
		pthread_mutex_lock(&pListUpdate); //lock mutex since we are editing
		playerList.erase(playerList.begin() + getPlayerIndex(sockfd)); //clear
		pthread_mutex_unlock(&pListUpdate);
	}
	cout << "Connection: " << sockfd << " closed" << endl;
	//return; //no return value
	
}


int main(int argc, char **argv){
    int sock, connfd;                /* Socket */
    struct sockaddr_in echoServAddr; /* Local address */
    struct sockaddr_in echoClntAddr; /* Client address */
    unsigned int cliAddrLen;         /* Length of incoming message */
    unsigned short echoServPort;     /* Server port */

    echoServPort = 36000;  //local port assigned for group number 35
	freePorts[0] = false;
	for(int x = 1; x < 1000; x++)
		freePorts[x] = true;

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
	
	string regName;
	pthread_t threadID;
	struct ThreadArgs *threadArgs;
	
	for(;;){ //loop the server indefinitely
		cliAddrLen = sizeof(echoClntAddr);
		connfd = accept( sock, (struct sockaddr *) &echoClntAddr, &cliAddrLen );
		
		cout << inet_ntoa(echoServAddr.sin_addr) << endl;
		cout << inet_ntoa(echoClntAddr.sin_addr) << endl;
		
		pthread_mutex_lock(&pListUpdate); //lock player list before modifying it
		regName = registerPlayer(connfd, inet_ntoa(echoClntAddr.sin_addr)); //register before creating next thread
		pthread_mutex_unlock(&pListUpdate); //unlock for further editing
		
		if(regName.compare("") != 0){
			
			printf("Handling client %s\n", inet_ntoa(echoClntAddr.sin_addr));
			//printf("on port %d\n", (echoServAddr.sin_port));
			threadArgs = (struct ThreadArgs *)malloc(sizeof(struct ThreadArgs));
			threadArgs -> sockfd = connfd;
			pthread_create(&threadID, NULL, serverInterface, (void *) threadArgs);
			
		}else{ //they disconnected very early
			close(connfd);
			cout << "Client failed to Regsiter" << endl;
		}
	}
}
