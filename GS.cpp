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

// TODO projeto:
// - fazer autoavaliação
// - fazer readme
// - fazer print to pdf dos testes e ter numa pasta autoavaliação, juntamente
// com o excel
// - não incluir shell.nix na entrega, não incluir hidden files na pasta
// - apagar message e response no player antes de entregar
// - garantir que zip do projeto extrai para a pasta atual com o unzip
//
// - fazer TODOs (prioridade é mensagens de erro)
// - reler o enunciado todo
// - update ao scoreboard
// - ter \n no fim das mensagens que o servidor recebe?
// - scoreboard e state do servidor não estão funcionais

// TODOs:
// - começar um jogo quando já está um a decorrer está partido?
// - ficheiro para cada jogo de cada player?
struct Game {
  string word;
  string hint_file;
  string current_word;
  int max_errors;
  int errors;
  int trial;
  set<char> letters;
  set<string> guessed_words;
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
  string word_file = "";

  if (argc % 2 != 0) {
    cout << "Error: invalid number of arguments provided." << endl;
    exit(1);
  }

  if (argc > 1) {
    word_file += argv[1];
  }

  if (word_file.length() <= 2) {
    cout << "Error: invalid word file provided." << endl;
    exit(1);
  }

  ifstream word_file_stream(word_file);
  if (!(word_file_stream = ifstream(word_file))) {
    cout << "Error: invalid word file provided." << endl;
    exit(1);
  }

  bool verbose = false;
  for (int i = 2; i < argc; i += 2) {
    if (strcmp(argv[i], "-p") == 0) {
      int port = atoi(argv[i + 1]);
      if (port > 65535 || port <= 1024) {
        cout << "Error: port argument in invalid range." << endl;
        exit(1);
      }
      GSport = argv[i + 1];
    } else if (strcmp(argv[i], "-v") == 0) {
      verbose = true;
    } else {
      cout << "Error: invalid argument provided." << endl;
      exit(1);
    }
  }

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

  while (true) {
    FD_SET(fd_udp, &fds);
    FD_SET(fd_tcp, &fds);

    if (select(max_fds, &fds, NULL, NULL, NULL) == -1) {
      cout << "An error occurred." << endl;
      exit(1);
    }

    if (FD_ISSET(fd_udp, &fds)) {
      Message message_struct = receive_message(fd_udp);

      if (verbose) {
        cout << "Message: ";
        for (string message_word : message_struct.message) {
          cout << message_word << " ";
        }
        cout << endl;
      }

      string keyword = message_struct.message[0];
      if (keyword == string("SNG")) {
        string response = "RSG ";
        string PLID = message_struct.message[1];
        if (is_valid_PLID(PLID)) {
          if (games.find(PLID) != games.end() && games[PLID].trial > 0) {
            response += "NOK";
          } else {
            string word_file_line;
            if (!getline(word_file_stream, word_file_line)) {
              word_file_stream.clear();
              word_file_stream.seekg(0, ios::beg);
              getline(word_file_stream, word_file_line);
            }

            if (verbose)
              cout << "word_file: " << word_file_line << endl;

            stringstream word_file_line_stream(word_file_line);

            word_file_line_stream >> games[PLID].word;
            word_file_line_stream >> games[PLID].hint_file;

            int word_length = (int)games[PLID].word.length();
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

            response += string("OK " + to_string(word_length) + " " +
                               to_string(max_errors));
          }
        } else {
          response += "ERR";
        }

        if (verbose) {
          cout << "RSG response: " << response << endl;
        }

        send_message(response, res_udp, fd_udp, message_struct.addr, false);
      } else if (keyword == string("PLG")) {
        string response = "RLG ";
        string PLID = message_struct.message[1];
        string trial_str = message_struct.message[3];
        int trial = stoi(trial_str);
        if (is_valid_PLID(PLID)) {
          if (games.find(PLID) != games.end()) {
            string letter_str = message_struct.message[2];
            if (letter_str.length() != 1) {
              response += "ERR" + trial_str;
            } else if (!isalpha(letter_str[0])) {
              response += "ERR" + trial_str;
            } else {
              char letter = letter_str[0];

              games[PLID].trial++;
              if (trial != games[PLID].trial) {
                response += "INV" + trial_str;
                games[PLID].trial--;
              } else {
                int n = 0;
                vector<int> pos;

                for (int i = 0; i < (int)games[PLID].word.length(); i++) {
                  if (games[PLID].word[i] == letter) {
                    n++;
                    pos.push_back(i + 1);
                    games[PLID].current_word[i] = letter;
                  }
                }

                if (games[PLID].current_word == games[PLID].word) {
                  response += "WIN " + trial_str;
                  games.erase(PLID);
                } else if (games[PLID].letters.count(letter) > 0) {
                  response += "DUP " + trial_str;
                  games[PLID].trial--;
                } else if (n == 0 &&
                           games[PLID].max_errors - games[PLID].errors > 0) {
                  response += "NOK " + trial_str;
                  games[PLID].errors++;
                  games[PLID].letters.insert(letter);
                } else if (n == 0 &&
                           games[PLID].max_errors - games[PLID].errors <= 0) {
                  response += "OVR " + trial_str;
                  games.erase(PLID);
                } else {
                  response += "OK " + trial_str + " " + to_string(n);
                  for (int p : pos) {
                    response += " " + to_string(p);
                  }
                  games[PLID].letters.insert(letter);
                }
              }
            }
          } else {
            response += "ERR" + trial_str;
          }
        } else {
          response += "ERR" + trial_str;
        }

        if (verbose) {
          cout << "RLG response: " << response << endl;
        }

        send_message(response, res_udp, fd_udp, message_struct.addr, false);
      } else if (keyword == string("PWG")) {
        string response = "RWG ";
        string PLID = message_struct.message[1];
        string guess_str = message_struct.message[2];
        string trial_str = message_struct.message[3];
        int trial = stoi(trial_str);

        if (is_valid_PLID(PLID)) {
          if (games.find(PLID) != games.end()) {
            if (guess_str.length() == games[PLID].word.length()) {
              for (int i = 0; i < (int)guess_str.length(); i++) {
                if (!isalpha(guess_str[i])) {
                  response += "ERR";
                  break;
                }
              }

              games[PLID].trial++;
              if (trial != games[PLID].trial) {
                response += "INV" + trial_str;
                games[PLID].trial--;

              } else {
                if (guess_str == games[PLID].word) {
                  response += "WIN " + to_string(trial);
                  games.erase(PLID);
                } else if (games[PLID].guessed_words.count(guess_str) > 0) {
                  response += "DUP " + to_string(trial);
                  games[PLID].trial--;
                } else if (guess_str != games[PLID].word &&
                           games[PLID].max_errors - games[PLID].errors > 0) {
                  response += "NOK " + to_string(trial);
                  games[PLID].errors++;
                  games[PLID].guessed_words.insert(guess_str);
                } else if (guess_str != games[PLID].word &&
                           games[PLID].max_errors - games[PLID].errors <= 0) {
                  response += "OVR " + to_string(trial);
                  games.erase(PLID);
                }
              }
            } else {
              response += "ERR " + to_string(trial);
            }
          } else {
            response += "ERR " + to_string(trial);
          }
        } else {
          response += "ERR" + to_string(trial);
        }

        if (verbose) {
          cout << "RWG response: " << response << endl;
        }

        send_message(response, res_udp, fd_udp, message_struct.addr, false);
      } else if (keyword == string("QUT")) {
        string response = "RQT ";
        string PLID = message_struct.message[1];
        if (is_valid_PLID(PLID)) {
          if (games.find(PLID) != games.end()) {
            games.erase(PLID);
            response += "OK";
          } else {
            response += "NOK";
          }
        } else {
          response += "ERR";
        }

        if (verbose) {
          cout << "RQT response: " << response << endl;
        }

        send_message(response, res_udp, fd_udp, message_struct.addr, false);
      } else {
        string response = "ERR";

        if (verbose) {
          cout << "ERR response: " << response << endl;
        }

        send_message(response, res_udp, fd_udp, message_struct.addr, false);
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
        if (verbose) {
          cout << "Message: ";
          for (string message_word : message_struct.message) {
            cout << message_word << " ";
          }
          cout << endl;
        }

        string keyword = message_struct.message[0];
        if (keyword == string("GSB")) {
          string response = string("RSB ");

          if (scoreboard.size() <= 4) {
            response += string("EMPTY");
          } else {
            string Fname = "scoreboard.txt";
            string Fdata = "";

            for (string line : scoreboard) {
              Fdata += line + "\n";
            }

            int Fsize = (int)Fdata.length();

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

            // TODO apagar e tirar is_tcp do send_message
            // send_message(response, res_udp, fd_udp, message_struct.addr,
            // false);
          }
        } else if (keyword == string("GHL")) {
          string response = string("RHL ");
          string PLID = message_struct.message[1];
          if (is_valid_PLID(PLID)) {
            string Fname = games[PLID].hint_file;
            string Fdata = "";

            ifstream file("hints/" + Fname, std::ios::binary);
            ostringstream stream;
            stream << file.rdbuf();
            Fdata = string(stream.str());
            int Fsize = Fdata.length();

            response +=
                string("OK " + Fname + " " + to_string(Fsize) + " " + Fdata);
          } else {
            response += "NOK";
          }

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
        } else if (keyword == string("STA")) {
          string response = string("RST ");
          string PLID = message_struct.message[1];
          if (is_valid_PLID(PLID)) {
            if (games.find(PLID) != games.end()) {
              string Fname = "STATE_" + PLID + ".txt"; // TODO
              string Fdata =
                  "Active game found for player " + PLID + "\nGame started - ";
              // TODO if transactions... else "no transactions found"
              Fdata += "\nSolved so far: " + games[PLID].current_word + "\n";

              // TODO guardar todas as jogadas do player (vector<string>) e
              // mostrar aqui

              int Fsize = Fdata.length();

              response +=
                  string("ACT " + Fname + " " + to_string(Fsize) + " " + Fdata);
            } else {
              // TODO no ongoing game mas já houve um jogo, most recent state
              response += "FIN";
            }
          } else {
            response += "NOK"; // TODO espaço a mais
          }

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
