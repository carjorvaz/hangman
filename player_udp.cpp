#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <string.h>

#include <iostream>
#include <sstream>
using namespace std;

string send_message(string str_msg, struct addrinfo *res, int fd) {
	ssize_t n;
	socklen_t addrlen;
	struct sockaddr_in addr;
	char buffer[128];

	const char *msg = (str_msg + "\n").c_str();
	n = sendto(fd, msg, strlen(msg), 0, res->ai_addr, res->ai_addrlen);
	if (n == -1) {
		cout << "An error ocurred" << endl;
		exit(1);
	}

	addrlen = sizeof(addr);
	n = recvfrom(fd, buffer, 128, 0,
			(struct sockaddr*) &addr, &addrlen);
	if (n == -1) {
		cout << "An error ocurred" << endl;
		exit(1);
	}

	return string(buffer); // TODO double-check if this is okay
}

int main(int argc, char** argv) {
	int fd, errcode;
	struct addrinfo hints, *res;

	int GN = 9;
	string GSIP = "0.0.0.0"; // IPv4
	string GSport = to_string(58000 + GN);

	string PLID;

	for (int i = 1; i < argc; i += 2) {
		if (strcmp(argv[i], "-n") == 0) {
			// TODO validate IPv4 with regex?
			// https://stackoverflow.com/questions/5284147/validating-ipv4-addresses-with-regexp
			GSIP = argv[i + 1];
		} else if (strcmp(argv[i], "-p") == 0) {
			int port = atoi(argv[i + 1]);
			if (port > 65535 || port <= 1024) {
				cout << "Error: port argument in invalid range" << endl;
				exit(-1);

			}
			GSport = argv[i + 1];
		}
	}

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd == -1) {
		cout << "An error ocurred" << endl;
		exit(1);
	}

	memset(&hints, 0, sizeof hints);

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;

	// TODO use command line arguments
	errcode = getaddrinfo("tejo.tecnico.ulisboa.pt", "58011", &hints, &res);
	if (errcode != 0) {
		cout << "An error ocurred" << endl;
		exit(1);
	}

	string input, keyword;
	while (getline(cin, input)) {
		stringstream line(input); // TODO check if any trash left after?
		line >> keyword;

		if (keyword == string("start") || keyword == string("sg")) {
			line >> PLID;
			if (PLID.length() != 6) {
					cout << "An error ocurred" << endl;
					exit(1);
			
			}

			for (char digit : PLID) {
				if (!('0' <= digit <= '9')) {		
					cout << "An error ocurred" << endl;
					exit(1);
				}
			
			}

			stringstream response(send_message("SNG " + PLID, res, fd));
			string response_component;

			response >> response_component;
			response >> response_component;
			if (response_component == string("OK")) {
				response >> response_component;
				cout << "New game started. Guess " << response_component << " letter word:";
				for (int i = 0; i < stoi(response_component); i++) {
					cout << " _";
				
				}

				cout << endl;
			} else {
				cout << "An error occurred" << endl;
			}
		} else if (keyword == string("play") || keyword == string("pl")) {
			string letter;
			line >> letter;


				
		}
	
	}

	// TODO Mudar daqui para baixo



	freeaddrinfo(res);
	close(fd);

	return 0;
}
