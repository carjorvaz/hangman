#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>
using namespace std;

// TODOs:
// - começar um jogo quando já está um a decorrer está partido?
// - ir buscar mais palavras?
// - ela faz o REV
struct Game {
  string word;
  string hint_file;
  string current_word;
  int max_errors;
  int errors;
  int trial;
  set<char> letters;
};

struct Message {
  vector<string> message;
  struct sockaddr_in addr;
};

Message receive_message(int fd) {
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

  Message message_struct;

  while (message_stream >> message_word) {
    message_struct.message.push_back(message_word);
  }

  message_struct.addr = addr;

  //
  cout << "Message: ";
  for (string message_word : message_struct.message) {
    cout << message_word << " ";
  }
  cout << endl;
  //

  return message_struct;
}

void send_message(string str_msg, struct addrinfo *res, int fd,
                  struct sockaddr_in addr, bool is_tcp) {
  ssize_t n;
  socklen_t addrlen;
  addrlen = sizeof(addr);

  char *msg = strdup((str_msg + "\n").c_str());
  n = sendto(fd, msg, strlen(msg), 0, (struct sockaddr *)&addr, addrlen);
  if (n == -1) {
    cout << "An error occurred while sending message." << endl;
    free(msg);
    exit(1);
  }

  free(msg);
}

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

  map<string, Game> games;
  while (true) {
    // TODO select entre UDP e TCP
    // TODO fork para cada TCP
    Message message_struct = receive_message(fd_udp);
    string keyword = message_struct.message[0];
    if (keyword == string("SNG")) {
      string PLID = message_struct.message[1];

      if (PLID.length() != 6) {
        // TODO mandar erro
      }

      for (char digit : PLID) {
        if (!isdigit(digit)) {
          // TODO mandar erro
        }
      }

      ifstream word_file_stream(word_file);
      string word_file_line;
      getline(word_file_stream, word_file_line);
      stringstream word_file_line_stream(word_file_line);

      word_file_line_stream >> games[PLID].word;
      word_file_line_stream >> games[PLID].hint_file;

      int word_length = games[PLID].word.length();
      int max_errors;
      if (word_length <= 6) {
        max_errors = 7;
      } else if (word_length >= 7 && word_length <= 10) {
        max_errors = 8;
      } else {
        max_errors = 9;
      }

      games[PLID].current_word = string(word_length, '_');
      games[PLID].max_errors = max_errors;
      games[PLID].errors = 0;
      games[PLID].trial = 0;

      string response = string("RSG OK " + to_string(word_length) + " " +
                               to_string(max_errors));
      send_message(response, res_udp, fd_udp, message_struct.addr, false);
    } else if (keyword == string("PLG")) {
      // TODO check if PLID exists in games set (if there's an ongoing game); ERR caso contrário
      // TODO confirmar PLID válido
      string PLID = message_struct.message[1];

      // TODO verificar que é só uma letra e que isalpha, criar char letter
      string letter_str = message_struct.message[2];
      char letter = letter_str[0];

      // TODO verificar que é um número de trial válido
      string trial_str = message_struct.message[3];
      int trial = stoi(trial_str);

      games[PLID].trial++;
      if (trial != games[PLID].trial) {
        // TODO erro INV
      }

      int n = 0;
      vector<int> pos;

      for (int i = 0; i < games[PLID].word.length(); i++) {
        if (games[PLID].word[i] == letter) {
          n++;
          pos.push_back(i + 1);
          games[PLID].current_word[i] = letter;
        }
      }

      string response = "RLG ";
      if (games[PLID].current_word == games[PLID].word) {
        response += "WIN " + to_string(trial);
        games.erase(PLID);
      } else if (games[PLID].letters.count(letter) > 0) {
        response += "DUP " + to_string(trial);
        games[PLID].trial--;
      } else if (n == 0 && games[PLID].max_errors - games[PLID].errors > 0) {
        response += "NOK " + to_string(trial);
        games[PLID].errors++;
      } else if (n == 0 && games[PLID].max_errors - games[PLID].errors <= 0) {
        response += "OVR " + to_string(trial);
        games.erase(PLID);
      } else {
        response += "OK " + to_string(trial) + " " + to_string(n);
        for (int p : pos) {
          response += " " + to_string(p);
        }
      }

      //
      cout << "response: " << response << endl;
      //

      // TODO adicionar letters ao letter quando falha e acerta
      send_message(response, res_udp, fd_udp, message_struct.addr, false);
    } else if (keyword == string("PWG")) {
    } else if (keyword == string("QUT")) {
    } else if (keyword == string("REV")) {
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
