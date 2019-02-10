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

#define ECHOMAX 255             /* Longest string to echo */
#define BACKLOG 128

using namespace std;

struct player{
	string name;
	string IP;
	string port;
};

void DieWithError(const char *errorMessage){ //included in sample code, carried over to project
        perror(errorMessage);
        exit(1);
}

void clientInput(FILE *fp, int sockfd){
	ssize_t n;
    char sendline[ECHOMAX], recvline[ECHOMAX];
	
	//know what command was sent & what return to expect
	string command = ""; //Register, Query, Start, Query, End, Deregister
	string value = "";
	string IP = "";
	string port = "";
	string output;
	string message;
	bool validInput;
	
	bool unreg = true;
	cout << "Please register: (formet Register <name> <IP> <port>" << endl;
	while(unreg && fgets(sendline, ECHOMAX, fp) != NULL){ //after connecting, register
		string message = string(sendline); //convert char message into string
		command = ""; //reset fields
		value = "";
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
				case 2: IP = part;
					break;
				case 3: port = part;
					break;
				default: //do nothing
					break;
			}
			count++;
		}while(iss);
		
		if(command.compare("register") == 0 || command.compare("Register") == 0){ //Register Player IP
			//add player to list
			validInput = true;
			if(value.compare("") != 0 && IP.compare("") != 0 && port.compare("") != 0){ //ensure all data was passed to server
				write(sockfd, sendline, strlen(sendline));
				//make sure server completed successfully
				memset(recvline, 0 ,255);
				if ( (n = read(sockfd, recvline, ECHOMAX) == 0) )
					DieWithError("str_cli: server terminated prematurely");
				string output = string(recvline);
				istringstream buf(output);
				int successField;
				buf >> successField;
				cout << successField << endl;
				if(successField == 0){
					cout << "Registered" << endl;
					unreg = false;
				}else{
					cout << "Failed to Register" << endl;
				}
			}else{
				cout << "Not all fields full" << endl;
			}
		}else{
			cout << "Incorrect command issued" << endl;
		}
	}
	
	//continuously loop
	while (fgets(sendline, ECHOMAX, fp) != NULL) {
		
		//translate snet command to anticipate return
		string message = string(sendline); //convert char message into string
		command = ""; //reset fields
		value = "";
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
				default: //do nothing
					break;
			}
			count++;
		}while(iss);
		
		//send a command to the server
		if(command.compare("register") != 0 && command.compare("Register") != 0){ //don't allow 1 client to register twice
			write(sockfd, sendline, strlen(sendline));
			if ( (n = read(sockfd, recvline, ECHOMAX) == 0) )
				DieWithError("str_cli: server terminated prematurely");
			output = string(recvline);
		}
		
		//interpret the command
		validInput = false;
		if(command.compare("register") == 0 || command.compare("Register") == 0){ //Register Player IP
			//add player to list
			cout << "You are already registered" << endl;
			
		}else if(command.compare("query") == 0 || command.compare("Query") == 0){ //Query Players or Games
			if(value.compare("players") == 0 || value.compare("Players") == 0){
				//expect list of players
				vector<string> triples;
				istringstream buffer(output);
				string part;
				while(getline(buffer, part, ' ')){
					triples.push_back(part);
				}
				int playerCount; //get an expected player count output
				istringstream convert(triples[0]); //first value will be palyer count
				convert >> playerCount;
				triples.erase(triples.begin()); //remove head
				int readPlayers = 0;
				while(!triples.empty()){ //read anything else in the buffer
					cout << "Player " << triples[0] << " at IP " << triples[1] << " and port " << triples[2] << endl;
					readPlayers++; //increment the player count
					triples.erase(triples.begin(), triples.begin()+3); //erase the read palyer
				}
				while(readPlayers < playerCount){ //repeat above for all players
					memset(recvline, 0, 255); //clean old entries in buffer
					if ( (n = read(sockfd, recvline, ECHOMAX) == 0) ) //rea anything in the buffer
						DieWithError("str_cli: server terminated prematurely");
					output = string(recvline);
					istringstream buffer(output);
					string part;
					while(getline(buffer, part, ' ')){ //break inputs by spaace & push to vector
						triples.push_back(part);
					}
					while(!triples.empty()){ //read triplets from vector & print to screen
						cout << "Player " << triples[0] << " at IP " << triples[1] << " and port " << triples[2] << endl;
						readPlayers++; //incremnet read player count
						triples.erase(triples.begin(), triples.begin()+3); //erase read palyers from vector
					}
				}
				
			}else if(value.compare("games") == 0 || value.compare("Games") == 0){
				//expect list of games
				cout << output << endl;
			}
			
		}else if(command.compare("start") == 0 || command.compare("Start") == 0){ //Start Game k
			//handle new game
			cout << output << endl;
			
		}else if(command.compare("end") == 0 || command.compare("End") == 0){ //End <Game ID>
			//end any client side game attributes
			cout << output << endl;
			
		}else if(command.compare("deregister") == 0 || command.compare("Deregister") == 0){ //Deregister Player
			//remove self, expected to end client soon after
			cout << output << endl;
		}else{
			cout << "Command unrecognized: " << output << endl;
		}
		//cout << "End process" << endl;
		memset(recvline, 0, 255); //clean buffer before receiving new input
		
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
