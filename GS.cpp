#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <iostream>
#include <sstream>
#include <vector>
using namespace std;

vector<string> receive_message(int fd) {
  ssize_t n;
  socklen_t addrlen;
  struct sockaddr_in addr;
  const int buffer_size = 128;
  char buffer[buffer_size];
  addrlen = sizeof(addr);

  n = recvfrom(fd, buffer, buffer_size, 0, (struct sockaddr *)&addr, &addrlen);
  if (n == -1) {
    cout << "An error occurred while receiving message." << endl;
    exit(1);
  }

  string buffer_str = "";
  buffer_str += string(buffer);
  stringstream message_stream(buffer_str);
  string message_word;

  vector<string> message;

  while (message_stream >> message_word) {
    message.push_back(message_word);
  }

  return message;
}

// TODO send_message
// n = sendto(fd, buffer, n, 0, (struct sockaddr *)&addr, addrlen);
// if (n == -1) {
//   printf("An error ocurred\n");
//   exit(1);
// }

int main(int argc, char **argv) {
  int fd_udp;
  int errcode;
  struct addrinfo hints_udp, *res_udp;

  int GN = 9;
  string GSport = to_string(58000 + GN);
  string word_file = argv[1]; // TODO rebenta se vazio
  bool verbose = false;       // TODO implementar verbose

  // TODO erro quando outras flags? aqui e no player
  for (int i = 2; i < argc; i += 2) {
    if (strcmp(argv[i], "-p") == 0) {
      int port = atoi(argv[i + 1]);
      if (port > 65535 || port <= 1024) {
        cout << "Error: port argument in invalid range" << endl;
        exit(-1);
      }
      GSport = argv[i + 1];
    } else if (strcmp(argv[i], "-v") == 0) {
      verbose = true;
    }
  }

  // TODO map de string (PLID) para struct com word, trial, errors, max_errors
  fd_udp = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd_udp == -1) {
    cout << "An error occurred." << endl;
    exit(1);
  }

  memset(&hints_udp, 0, sizeof hints_udp);
  hints_udp.ai_family = AF_INET;
  hints_udp.ai_socktype = SOCK_DGRAM;
  hints_udp.ai_flags = AI_PASSIVE;
  errcode = getaddrinfo(NULL, GSport.c_str(), &hints_udp, &res_udp);
  if (errcode != 0) {
    cout << "An error occurred while estabilishing connection." << endl;
    exit(1);
  }

  errcode = bind(fd_udp, res_udp->ai_addr, res_udp->ai_addrlen);
  if (errcode == -1) {
    cout << "An error occurred while binding to port." << endl;
    exit(1);
  }

  char buffer[128]; // TODO buffer_size
  while (true) {
    // TODO select entre UDP e TCP
    // TODO fork para cada TCP
    vector<string> message = receive_message(fd_udp);

    //
    cout << "Message: ";
    for (string message_word : message) {
      cout << message_word << " ";
    }
    cout << endl;
    //

    if (message[0] == string("SNG")) {
      string PLID = message[1];

      if (PLID.length() != 6) {
        // TODO mandar erro
      }

      for (char digit : PLID) {
        if (!isdigit(digit)) {
          // TODO mandar erro
        }
      }

      // TODO escolher palavra
      // TODO número de erros
    }
  }

  freeaddrinfo(res_udp);
  close(fd_udp);

  return 0;

  // // Abaixo é template (tcp)
  // fd = socket(AF_INET, SOCK_STREAM, 0);
  // if (fd == -1) {
  //   cout << "An error occurred." << endl;
  //   exit(1);
  // }

  // memset(&hints, 0, sizeof hints);

  // hints.ai_family = AF_INET;
  // hints.ai_socktype = SOCK_STREAM;
  // hints.ai_flags = AI_PASSIVE;

  // errcode = getaddrinfo(NULL, GSport.c_str(), &hints, &res);
  // if (errcode != 0) {
  //   cout << "An error occurred." << endl;
  //   exit(1);
  // }

  // n = bind(fd, res->ai_addr, res->ai_addrlen);
  // if (n == -1) {
  //   cout << "An error occurred." << endl;
  //   exit(1);
  // }

  // if (listen(fd, 5) == -1) {
  //   cout << "An error occurred." << endl;
  //   exit(1);
  // }

  // while (1) {
  //   addrlen = sizeof(addr);
  //   if ((newfd = accept(fd, (struct sockaddr *)&addr, &addrlen)) == -1) {
  //     cout << "An error occurred." << endl;
  //     exit(1);
  //   }

  //   n = read(newfd, buffer, 128);
  //   if (n == -1) {
  //     cout << "An error occurred." << endl;
  //     exit(1);
  //   }

  //   buffer[n] = '\0';

  //   write(1, "received: ", 10);
  //   write(1, buffer, n);

  //   n = write(newfd, buffer, strlen(buffer));
  //   if (n == -1) {
  //     cout << "An error occurred." << endl;
  //     exit(1);
  //   }

  //   close(newfd);
  // }

  // freeaddrinfo(res);
  // close(fd);

  // return 0;
}
