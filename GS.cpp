#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
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
// - ela faz o REV; nao ha rev
// - ficheiro para cada jogo de cada player?
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

bool is_valid_PLID(string PLID) {
  if (PLID.length() != 6) {
    return false;
  }

  for (char digit : PLID) {
    if (!isdigit(digit)) {
      return false;
    }
  }

  return true;
}

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
  int fd_tcp;
  fd_set fds;
  int errcode;
  struct addrinfo hints_udp, *res_udp;
  struct addrinfo hints_tcp, *res_tcp;
  socklen_t player_addr;
  socklen_t player_addrlen;

  int GN = 9;
  string GSport = to_string(58000 + GN);
  string word_file = argv[1]; // TODO rebenta se vazio
  ifstream word_file_stream(word_file);
  bool verbose = false; // TODO implementar verbose

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

  fd_tcp = socket(AF_INET, SOCK_STREAM, 0);
  if (fd_tcp == -1) {
    cout << "An error occurred." << endl;
    exit(1);
  }

  memset(&hints_udp, 0, sizeof hints_udp);
  hints_udp.ai_family = AF_INET;
  hints_udp.ai_socktype = SOCK_DGRAM;
  hints_udp.ai_flags = AI_PASSIVE;

  memset(&hints_tcp, 0, sizeof hints_tcp);
  hints_tcp.ai_family = AF_INET;
  hints_tcp.ai_socktype = SOCK_STREAM;
  hints_tcp.ai_flags = AI_PASSIVE;

  errcode = getaddrinfo(NULL, GSport.c_str(), &hints_udp, &res_udp);
  if (errcode != 0) {
    cout << "An error occurred while estabilishing connection." << endl;
    exit(1);
  }

  errcode = getaddrinfo(NULL, GSport.c_str(), &hints_tcp, &res_tcp);
  if (errcode != 0) {
    cout << "An error occurred while estabilishing connection." << endl;
    exit(1);
  }

  errcode = bind(fd_udp, res_udp->ai_addr, res_udp->ai_addrlen);
  if (errcode == -1) {
    cout << "An error occurred while binding to port." << endl;
    exit(1);
  }

  errcode = bind(fd_tcp, res_tcp->ai_addr, res_tcp->ai_addrlen);
  if (errcode == -1) {
    cout << "An error occurred while binding to port." << endl;
    exit(1);
  }

  if (listen(fd_tcp, 5) == -1) {
    cout << "An error occurred." << endl;
    exit(1);
  }

  FD_ZERO(&fds);
  int max_fds = max(fd_udp, fd_tcp) + 1;
  map<string, Game> games;
  vector<string> scoreboard;
  scoreboard.push_back(string("-------------------------------- TOP 10 SCORES "
                              "--------------------------------"));
  scoreboard.push_back(string(""));
  scoreboard.push_back(string("    SCORE PLAYER     WORD                       "
                              "      GOOD TRIALS  TOTAL TRIALS"));
  scoreboard.push_back(string(""));

  int ready;
  while (true) {
    // TODO é preciso fd_zero?
    FD_SET(fd_udp, &fds);
    FD_SET(fd_tcp, &fds);

    ready = select(max_fds, &fds, NULL, NULL, NULL);

    if (FD_ISSET(fd_udp, &fds)) {
      Message message_struct = receive_message(fd_udp);
      string keyword = message_struct.message[0];
      if (keyword == string("SNG")) {
        string PLID = message_struct.message[1];
        if (is_valid_PLID(PLID)) {
          string word_file_line;
          getline(word_file_stream, word_file_line);
          //
          cout << "word_file_line: " << word_file_line << endl;
          //
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

          // TODO RSG NOK se já tiver começado jogo e já tiver feito uma jogada

          string response = string("RSG OK " + to_string(word_length) + " " +
                                   to_string(max_errors));
          send_message(response, res_udp, fd_udp, message_struct.addr, false);
        } else {
          // TODO err invalid plid
        }
      } else if (keyword == string("PLG")) {
        string PLID = message_struct.message[1];
        if (is_valid_PLID(PLID)) {
          if (games.find(PLID) != games.end()) {
            // TODO verificar que é só uma letra e que isalpha, criar char
            // letter
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
            } else if (n == 0 &&
                       games[PLID].max_errors - games[PLID].errors > 0) {
              response += "NOK " + to_string(trial);
              games[PLID].errors++;
              games[PLID].letters.insert(letter);
            } else if (n == 0 &&
                       games[PLID].max_errors - games[PLID].errors <= 0) {
              response += "OVR " + to_string(trial);
              games.erase(PLID);
            } else {
              response += "OK " + to_string(trial) + " " + to_string(n);
              for (int p : pos) {
                response += " " + to_string(p);
              }
              games[PLID].letters.insert(letter);
            }

            // TODO adicionar letters ao letter quando falha e acerta
            send_message(response, res_udp, fd_udp, message_struct.addr, false);
          } else {
            // TODO ERR não há jogo
          }
        } else {
          // TODO ERR PLID Inválido
        }
      } else if (keyword == string("PWG")) {
      } else if (keyword == string("QUT")) {
        string PLID = message_struct.message[1];
        if (is_valid_PLID(PLID)) {
          string response = "RQT ";
          if (games.find(PLID) != games.end()) {
            games.erase(PLID);
            response += "OK";
          } else {
            response += "NOK";
          }

          send_message(response, res_udp, fd_udp, message_struct.addr, false);
        } else {
          // TODO err invalid plid
        }
      } else {
        // TODO err invalid command
      }
    }

    if (FD_ISSET(fd_tcp, &fds)) {
      player_addrlen = sizeof(player_addr);
      int player_fd =
          accept(fd_tcp, (struct sockaddr *)&player_addr, &player_addrlen);
      int child_pid;
      if ((child_pid = fork()) == 0) {
        close(fd_tcp);
        Message message_struct =
            receive_message(player_fd); // TODO mudar, fazer tudo aqui
                                        // é para ser read em vez de recvfrom,
                                        // pode ser partido aos bocados
        string keyword = message_struct.message[0];
        if (keyword == string("GSB")) {
          string response = string("RSB ");

          if (scoreboard.size() <= 3) { // TODO mudar para 4
            response +=
                string("EMPTY "); // TODO não é suposto haver espaço no fim mas
                                  // read_word do player está à espera de um
                                  // espaço, arranjar se houver tempo
          } else {
            string Fname = "scoreboard.txt";
            string Fdata = "";

            for (string line : scoreboard) {
              Fdata += line + "\n";
            }

            int Fsize = Fdata.length();

            response +=
                string("OK " + Fname + " " + to_string(Fsize) + " " + Fdata);
            // TODO ordenar scoreboard, usar index para fazer o 1 -; acrescentar
            // à scoreboard no WIN
            // TODO efetivamente acrescentar coisas à scoreboard, ter atenção
            // aos tabs para ficar alinhado
            int n;
            ssize_t bytes_written = 0;
            while (true) {
              n = write(player_fd, response.c_str() + bytes_written,
                        response.length() - bytes_written);
              if (n == -1) {
                cout << "An error occurred." << endl;
                exit(1);
              } else if (n == 0) {
                break;
              }

              bytes_written += n;
            }
          }

          // TODO apagar e tirar is_tcp do send_message
          // send_message(response, res_udp, fd_udp, message_struct.addr,
          // false);
        }

        close(player_fd);
        exit(0);
      }
      close(player_fd);
    }
  }

  freeaddrinfo(res_udp);
  freeaddrinfo(res_tcp);
  close(fd_udp);
  close(fd_tcp);

  return 0;
}
