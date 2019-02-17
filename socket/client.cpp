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
	char port[ECHOMAX]; //this doubles as the game ID for end game
	int pCount;
	int serverSocket; //where to send "end game" command
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
	cout << "Please Register: (Register <player name>)" << endl;
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
string myName; //shared across input & P2P threads

void *clientInput(void *threadArg){
	int sockfd, port, count;
	string message, IP, command, value, part;
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
				cout << "There are " << playerCount << " registered players" << endl;
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
				if(toPrint.size() == 0){ //need to ensure we have the game count in toPrint before continuing
					readInput(sockfd);
				}
				//read game List length
				int gameCount;
				istringstream convert(toPrint[0]);
				convert >> gameCount;
				toPrint.erase(toPrint.begin());
				cout << "There are " << gameCount << " games in session" << endl;
				int readGames;
				while(readGames < gameCount){
					if(toPrint.size() == 0){ //need to ensure this isn't empty
						readInput(sockfd);
					}
					cout << "Game #" << toPrint[0] << " hosted by " << toPrint[1] << " has players:" << endl; //toPrint should be the ID & caller 1st
					toPrint.erase(toPrint.begin());
					toPrint.erase(toPrint.begin()); //clean those fields
					bool hasPlayers = true;
					while(hasPlayers){ //still has another player in session
						if(toPrint.size() == 0){ //need to ensure this isn't empty
							readInput(sockfd);
						}
						if(toPrint[0].compare("next") == 0){
							hasPlayers = false;
						}else{
							cout << toPrint[0] << endl;
							toPrint.erase(toPrint.begin());
						}
					}
					toPrint.erase(toPrint.begin()); //erase "next" from list
					readGames++; //that is the whole game info, increment
				}
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
				gameDet -> serverSocket = sockfd;
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
	string IP, message, part;
	int pCount, returnSocket;
	ssize_t n;
	unsigned short hostPort;
	pthread_detach(pthread_self());
	IP = string(((struct P2Pargs *) P2Parg ) -> IP); //convert args back to string
	hostPort = (unsigned short)strtoul(((struct P2Pargs *) P2Parg ) -> port, NULL ,0);
	pCount = ((struct P2Pargs *) P2Parg ) -> pCount;
	returnSocket = ((struct P2Pargs *) P2Parg ) -> serverSocket; //where to send end command at completion
	free(P2Parg);
	
	int sock, connfd;                /* Socket */
    struct sockaddr_in hostAddr; /* Local address */
    struct sockaddr_in playAddr; /* Client address */
    unsigned int cliAddrLen;         /* Length of incoming message */
	
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        DieWithError("server: socket() failed");

    /* Construct local address structure */
    memset(&hostAddr, 0, sizeof(hostAddr));   /* Zero out structure */
    hostAddr.sin_family = AF_INET;                /* Internet address family */
    hostAddr.sin_addr.s_addr = htonl(INADDR_ANY); /* Any incoming interface */
    hostAddr.sin_port = htons(hostPort);      /* Local port */
	
	if (bind(sock, (struct sockaddr *) &hostAddr, sizeof(hostAddr)) < 0)
        DieWithError("server: bind() failed");
	
	if (listen(sock, BACKLOG) < 0 )
		DieWithError("server: listen() failed");
	
	int sockfd[pCount]; //store all sockets connceting for round robin style BINGO calling
	
	for(int x = 0; x < pCount; x++){ //loop until all expected players have connected
		cliAddrLen = sizeof(playAddr);
		sockfd[x] = accept( sock, (struct sockaddr *) &playAddr, &cliAddrLen ); //accept n many connections
	}
	
	bool stillPlaying = true;
	int randLet; //ranges 0-4
	int randNum; //ranges 1-15, modified by called letter (ie, 1 3 = B (3 + 0*15), & 2 3 is I (3 + 1*15)
	while(stillPlaying){
		//make the call
		randLet = rand() % 5; //[0,4]
		randNum = rand() % 15 + 1; //[1,15]
		randNum = randNum + (randLet * 15);
		stringstream buffer; //convert rand num to a string
		buffer << randNum;
		switch(randLet){
			case 0: message = "B " + buffer.str() + " ";
				break;
			case 1: message = "I " + buffer.str() + " ";
				break;
			case 2: message = "N " + buffer.str() + " ";
				break;
			case 3: message = "G " + buffer.str() + " ";
				break;
			case 4: message = "O " + buffer.str() + " ";
				break;
		}
		
		//send call to all players
		for(int x = 0; x < pCount; x++){
			write(sockfd[x], message.c_str(), message.size());
		}
		
		//wait for affirmation
		bool printName = false; //used to see if a victor sent their name & can be reset if multiple winners
		for(int x = 0; x < pCount; x++){
			printName = false;
			memset(playLine, 0, 255); //clear last returned message
			if ( (n = read(sockfd[x], playLine, ECHOMAX) == 0) ) //client disconnected illegally, we gonna die
				DieWithError("str_cli: client terminated prematurely");
			message = string(playLine); //expect either "Recv " or "Win <pName> "
			istringstream buf(message);
			while(getline(buf, part, ' ')){ //read everything from recvline & push back delimited by spaces
				if(printName == true){
					cout << part << " has called BINGO" << endl;
				}
				if(part.compare("Win") == 0){ //some one declared victory
					stillPlaying = false;
					printName = true;
				}
			}
		}
		//printf("Handling client %s\n", inet_ntoa(playAddr.sin_addr));
		//close(connfd);
	}
	//game has ended, tell all players to disconnected
	for(int x = 0; x < pCount; x++){
		message = "X "; //this will be the end message, since there is no X in BINGO
		write(sockfd[x], message.c_str(), message.size());
	}
	
	//wait for all players to say they've ended
	int counter = 0;
	bool stillOpen = true;
	for(int z = 0; z < pCount; z++){
		stillOpen = true;
		while(stillOpen){
			memset(playLine, 0, 255); //clean buffer before receiving next
			if ( (n = read(sockfd[z], playLine, ECHOMAX) == 0) ) //client disconnected illegally, we gonna die
				DieWithError("str_cli: client terminated prematurely");
			message = string(playLine);
			if(message.compare("End") == 0);
				stillOpen = false;
		}
	}
	stringstream ss;
	ss << hostPort;
	message = "End " + ss.str() + " ";
	write(returnSocket, message.c_str(), message.size());
	close(sock);
	pthread_exit(NULL);
}

bool checkWin(bool board[5][5]){
	bool win = false;
	//check rows
	for(int row = 0; row < 5; row++){
		if(board[row][0] && board[row][1] && board[row][2] && board[row][3] && board[row][4])
			win = true;
	}
	//check columns
	for(int column = 0; column < 5; column++){
		if(board[0][column] && board[1][column] && board[2][column] && board[3][column] && board[4][column])
			win = true;
	}
	//check diagonals
	if(board[0][0] && board[1][1] && board[2][2] && board[3][3] && board[4][4])
		win = true;
	if(board[0][4] && board[1][3] && board[2][2] && board[3][1] && board[4][0])
		win = true;
	
	return win;
}

void printBINGO(bool called[5][5], int val[5][5]){
	cout << "B\tI\tN\tG\tO" << endl;
	for(int row = 0; row < 5; row++){
		for(int column = 0; column < 5; column++){
			if(row == 2 && column == 2){
				cout << "Free\t";
			}else if(called[row][column]){
				cout << "X " << val[row][column] << "\t"; 
			}else{
				cout << "O " << val[row][column] << "\t";
			}
		}
		cout << endl;
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
	string message, part;
	int myBoard[5][5]; //BINGO & nums
	for(int x = 0; x < 5; x++){ //random the board
		int r1 = 0,r2 = 0,r3 = 0,r4 = 0,r5 = 0;
		while(r1 == r2 || r1 == r3 || r1 == r4 || r1 == r5 || r2 == r3 || r2 == r4 || r2 == r5 || r3 == r4 || r3 == r5 || r4 == r5){ //ensure no duplicates
			r1 = (rand() % 15 + 1) + (x * 15);
			r2 = (rand() % 15 + 1) + (x * 15);
			r3 = (rand() % 15 + 1) + (x * 15);
			r4 = (rand() % 15 + 1) + (x * 15);
			r5 = (rand() % 15 + 1) + (x * 15);
		}
		myBoard[0][x] = r1;
		myBoard[1][x] = r2;
		myBoard[2][x] = r3;
		myBoard[3][x] = r4;
		myBoard[4][x] = r5;
	}
	myBoard[2][2] = 0; //free space
	bool called[5][5]; //bool for occupied
	for(int x = 0; x < 5; x++)
		for(int y = 0; y < 5; y++)
			called[x][y] = false;
	called[2][2] = true; //free space
	string letCall;
	int numCall;
	int partCount = 1;
	ssize_t n;
	bool stillPlaying = true;
	int randEnd;
	while(stillPlaying){
		memset(playLine, 0, 255); //clean buffer before receiving next
		if ( (n = read(sockfd, playLine, ECHOMAX) == 0) )
			DieWithError("str_cli: server terminated prematurely");
		message = string(playLine);
		istringstream buffer(message); //break apart message, should be 2 parts tops
		partCount = 1;
		while(getline(buffer, part, ' ')){ //read everything from recvline & push back delimited by spaces
			if(partCount == 1){
				cout << part << " ";
				letCall = part;
				if(part.compare("X") == 0)
					stillPlaying = false;
				partCount++;
			}else if(partCount == 2){
				cout << part << " ";
				stringstream buf(part);
				buf >> numCall;
			}
			
		}
		cout << endl;
		if(stillPlaying){ //no winner, update board
			if(letCall.compare("B") == 0){ //check if square was called
				for(int x = 0; x < 5; x++){
					if(myBoard[x][0] == numCall){
						called[x][0] = true;
					}
				}
			}else if(letCall.compare("I") == 0){
				for(int x = 0; x < 5; x++){
					if(myBoard[x][1] == numCall){
						called[x][1] = true;
					}
				}
			}else if(letCall.compare("N") == 0){
				for(int x = 0; x < 5; x++){
					if(myBoard[x][2] == numCall){
						called[x][2] = true;
					}
				}
			}else if(letCall.compare("G") == 0){
				for(int x = 0; x < 5; x++){
					if(myBoard[x][3] == numCall){
						called[x][3] = true;
					}
				}
			}else if(letCall.compare("O") == 0){
				for(int x = 0; x < 5; x++){
					if(myBoard[x][4] == numCall){
						called[x][4] = true;
					}
				}
			}
			
			printBINGO(called, myBoard); //print current board state
			if(checkWin(called)) //see if you've won
				message = "Win " + myName + " ";
			else //game continues
				message = "Recv ";
			write(sockfd, message.c_str(), message.size()); //tell host if you've won or not
		}
		
	}
	write(sockfd, "End", 4); //some one won, tell host you are successfully disconnecting
	cout << "END" << endl;
	pthread_exit(NULL);
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
