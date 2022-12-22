#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <ctype.h>
#include <string.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
using namespace std;

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

string read_word(int fd) {
  int n;
  char buffer_tcp[1];
  string word = "";

  while (true) {
    n = read(fd, buffer_tcp, 1);
    if (n == -1) {
      cout << "An error occurred." << endl;
      exit(1);
    }

    if (buffer_tcp[0] == ' ' || buffer_tcp[0] == '\n') {
      break;
    } else {
      word += buffer_tcp[0];
    }
  }

  return word;
}

vector<string> send_message(string str_msg, struct addrinfo *res, int fd,
                            bool is_tcp) {
  ssize_t n;
  socklen_t addrlen;
  struct sockaddr_in addr;
  const int buffer_size = 128;
  char buffer[buffer_size];
  addrlen = sizeof(addr);

  vector<string> response;

  char *msg = strdup((str_msg + "\n").c_str());
  n = sendto(fd, msg, strlen(msg), 0, res->ai_addr, res->ai_addrlen);
  if (n == -1) {
    cout << "An error occurred while sending message." << endl;
    free(msg);
    exit(1);
  }

  free(msg);

  if (!is_tcp) {
    n = recvfrom(fd, buffer, buffer_size, 0, (struct sockaddr *)&addr,
                 &addrlen);
    if (n == -1) {
      cout << "An error occurred." << endl;
      exit(1);
    }

    string buffer_str = "";
    buffer_str += string(buffer);
    stringstream response_stream(buffer_str);
    string response_word;

    response_stream >> response_word;
    response.push_back(response_word);
    while (response_stream >> response_word) {
      response.push_back(response_word);
    }
  } else {
    string response_word = read_word(fd);
    if (response_word.length() == 0) {
      cout << "An error occurred while trying to read from socket." << endl;
    } else {
      response.push_back(response_word);
      if (response_word == string("RSB") || response_word == string("RHL") ||
          response_word == string("RST")) {
        string status = read_word(fd);
        if (status.length() == 0) {
          cout << "An error occurred while trying to read from socket." << endl;
        } else {
          response.push_back(status);
          if (status == string("OK") || status == string("ACT") ||
              status == string("FIN")) {
            string Fname = read_word(fd);
            response.push_back(Fname);

            string Fsize = read_word(fd);
            response.push_back(Fsize);

            int tcp_buffer_size = stoi(Fsize) * sizeof(char);
            char *tcp_buffer = (char *)malloc(tcp_buffer_size);
            ssize_t bytes_read = 0;

            while (true) {
              n = read(fd, tcp_buffer + bytes_read,
                       tcp_buffer_size - bytes_read);
              if (n == -1) {
                cout << "An error occurred" << endl;
                exit(1);
              } else if (n == 0) {
                break;
              }

              bytes_read += n;
            }

            vector<char> Fdata(tcp_buffer, tcp_buffer + tcp_buffer_size);

            ofstream file(Fname, ios::out | ios::binary);
            file.write((char *)&Fdata[0], Fdata.size() * sizeof(char));
            file.close();

            if (response_word == string("RSB")) {
              cout << "Fetched scoreboard successfully." << endl
                   << string(tcp_buffer) << "File name: " << Fname
                   << ", file size: " << Fsize << "B." << endl
                   << endl;
            } else if (response_word == string("RHL")) {
              cout << "Fetched hint sucessfully." << endl
                   << "File name: " << Fname << "." << endl
                   << endl;
            } else if (response_word == string("RST")) {
              cout << "Fetched game state successfully." << endl
                   << endl
                   << string(tcp_buffer) << endl
                   << "File name: " << Fname << "." << endl
                   << endl;
            }

            free(tcp_buffer);
          } else {
            response.push_back(status);
          }
        }
      }
    }
  }

  return response;
}

int main(int argc, char **argv) {
  int errcode;
  int fd_udp;
  struct addrinfo hints_udp, *res_udp;
  struct addrinfo hints_tcp, *res_tcp;

  int GN = 9;
  string GSIP = "0.0.0.0";
  string GSport = to_string(58000 + GN);

  string PLID;
  string word;
  int trial = 0;
  int errors = 0;
  int max_errors = 0;
  bool game_running = false;

  if (argc % 2 != 1) {
    cout << "Error: invalid number of arguments provided." << endl;
    exit(1);
  }

  for (int i = 1; i < argc; i += 2) {
    if (strcmp(argv[i], "-n") == 0) {
      GSIP = argv[i + 1];
    } else if (strcmp(argv[i], "-p") == 0) {
      int port = atoi(argv[i + 1]);
      if (port > 65535 || port <= 1024) {
        cout << "Error: port argument in invalid range" << endl;
        exit(1);
      }
      GSport = argv[i + 1];
    } else {
      cout << "Error: invalid argument provided." << endl;
      exit(1);
    }
  }

  fd_udp = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd_udp == -1) {
    cout << "An error occurred while creating socket." << endl;
    exit(1);
  }

  memset(&hints_udp, 0, sizeof hints_udp);
  hints_udp.ai_family = AF_INET;
  hints_udp.ai_socktype = SOCK_DGRAM;
  errcode = getaddrinfo(GSIP.c_str(), GSport.c_str(), &hints_udp, &res_udp);
  if (errcode != 0) {
    cout << "An error occurred while getting address info." << endl;
    exit(1);
  }

  memset(&hints_tcp, 0, sizeof hints_tcp);
  hints_tcp.ai_family = AF_INET;
  hints_tcp.ai_socktype = SOCK_STREAM;
  errcode = getaddrinfo(GSIP.c_str(), GSport.c_str(), &hints_tcp, &res_tcp);
  if (errcode != 0) {
    cout << "An error occurred while getting address info." << endl;
    exit(1);
  }

  string input_line, keyword;
  while (cout << "> " && getline(cin, input_line)) {
    stringstream line_stream(input_line);
    string command;
    line_stream >> command;

    if (command == string("start") || command == string("sg")) {
      line_stream >> PLID;
      if (is_valid_PLID(PLID)) {
        vector<string> response =
            send_message("SNG " + PLID, res_udp, fd_udp, false);
        string status = response[1];
        if (status == string("OK")) {
          int n_letters = stoi(response[2]);
          max_errors = stoi(response[3]);

          word = string(n_letters, '_');
          trial = 1;
          errors = 0;
          game_running = true;

          cout << "New game started! Guess " << n_letters << " letter word: ";
          for (int i = 0; i < n_letters; i++) {
            cout << "_";
          }
          cout << endl
               << "You can make up to " << max_errors << " errors." << endl
               << endl;
        } else {
          cout << "An error occurred. Is another game currently running?"
               << endl
               << endl;
        }
      } else {
        cout << "Invalid PLID provided. Please try again." << endl;
      }
    } else if (command == string("play") || command == string("pl")) {
      string letter;
      line_stream >> letter;
      if (letter.length() != 1) {
        cout << "Please enter one letter only. Try again." << endl << endl;
      } else if (!isalpha(letter[0])) {
        cout << "Please enter a letter. Try again." << endl << endl;
      } else {
        if (isupper(letter[0])) {
          letter[0] = tolower(letter[0]);
        }

        if (max_errors - errors > 0) {
          string message =
              string("PLG " + PLID + " " + letter + " " + to_string(trial));
          vector<string> response =
              send_message(message, res_udp, fd_udp, false);

          string status = response[1];
          if (status == string("OK")) {
            int n = stoi(response[3]);
            for (int i = 0; i < n; i++) {
              int pos = stoi(response[4 + i]);
              word[pos - 1] = letter[0];
            }

            trial++;

            cout << "Correct guess! Current word: " << word << endl << endl;
          } else if (status == string("WIN")) {
            for (int i = 0; i < (int)word.length(); i++) {
              if (word[i] == '_')
                word[i] = letter[0];
            }

            cout << "You guessed it! The word is " << word << "!" << endl
                 << endl;
            game_running = false;
          } else if (status == string("DUP")) {
            cout << "Duplicate guess, try again." << endl << endl;
          } else if (status == string("NOK")) {
            trial++;
            errors++;

            cout << "The letter " << letter << " is not part of the word."
                 << endl
                 << endl;
          } else if (status == string("OVR")) {
            cout << "The letter " << letter
                 << " is not part of the word. You have no guesses left."
                 << "." << endl
                 << endl;
          } else if (status == string("INV")) {
            cout << "Invalid trial number. Contact your local client's "
                    "developer."
                 << endl
                 << endl;
          } else if (status == string("ERR")) {
            cout << "Error. Have you started the game yet?" << endl << endl;
          } else {
            cout << "Unexpected error." << endl << endl;
          }
        } else {
          cout
              << "Either the game has not started or you have no attempts left."
              << endl
              << endl;
          game_running = false;
        }
      }
    } else if (command == string("guess") || command == string("gw")) {
      bool valid = true;
      string guessed_word;
      line_stream >> guessed_word;

      for (char guessed_word_letter : guessed_word) {
        if (!isalpha(guessed_word_letter)) {
          valid = false;
          cout << "Word may only contain letters. Try again." << endl << endl;
          break;
        }
      }

      if (valid && guessed_word.length() != word.length()) {
        valid = false;
        cout << "Wrong word size. Try again." << endl << endl;
      }

      if (valid) {
        if (max_errors - errors > 0) {
          string message = string("PWG " + PLID + " " + guessed_word + " " +
                                  to_string(trial));
          vector<string> response =
              send_message(message, res_udp, fd_udp, false);

          if (response[1] == string("WIN")) {
            word = guessed_word;
            cout << "You guessed it! The word is indeed " << word << "!" << endl
                 << endl;
            game_running = false;
          } else if (response[1] == string("DUP")) {
            cout << "Duplicate guess, try again." << endl << endl;
          } else if (response[1] == string("NOK")) {
            trial++;
            errors++;
            cout << "Wrong guess. Try again." << endl << endl;
          } else if (response[1] == string("OVR")) {
            cout << "Wrong guess. You have no guesses left."
                 << "." << endl
                 << endl;
          } else if (response[1] == string("INV")) {
            cout << "Invalid trial number. Contact your local client's "
                    "developer."
                 << endl
                 << endl;
          } else if (response[1] == string("ERR")) {
            cout << "Either the word was the wrong length or you haven't "
                    "started the game yet. Try again."
                 << endl
                 << endl;
          }
        } else {
          cout << "You have no attempts left. Game over." << endl << endl;
          game_running = false;
        }
      }
    } else if (command == string("scoreboard") || command == string("sb")) {
      int fd = socket(AF_INET, SOCK_STREAM, 0);
      if (fd == -1) {
        cout << "An error occurred while creating socket." << endl;
      } else if (connect(fd, res_tcp->ai_addr, res_tcp->ai_addrlen) == -1) {
        cout << "An error occurred while connecting to socket." << endl;
      } else {
        string message = string("GSB");
        vector<string> response = send_message(message, res_tcp, fd, true);

        string keyword = response[0];
        string status = response[1];

        if (status == string("EMPTY")) {
          cout << "No game was yet won by any player." << endl << endl;
        }
      }

      close(fd);
    } else if (command == string("hint") || command == string("h")) {
      int fd;
      if (!game_running) {
        cout << "No game is currently running." << endl;
      } else if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        cout << "An error occurred while creating socket." << endl;
        close(fd);
      } else if (connect(fd, res_tcp->ai_addr, res_tcp->ai_addrlen) == -1) {
        cout << "An error occurred while connecting to socket." << endl;
        close(fd);
      } else {
        string message = string("GHL " + PLID);
        vector<string> response = send_message(message, res_tcp, fd, true);

        string keyword = response[0];
        string status = response[1];
        if (status == string("NOK")) {
          cout << "An error occurred while receiving hint." << endl << endl;
        }
        close(fd);
      }
    } else if (command == string("state") || command == string("st")) {
      int fd;
      if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        cout << "An error occurred while creating socket." << endl;
        close(fd);
      } else if (connect(fd, res_tcp->ai_addr, res_tcp->ai_addrlen) == -1) {
        cout << "An error occurred while connecting to socket." << endl;
        close(fd);
      } else {
        string message = string("STA " + PLID);
        vector<string> response = send_message(message, res_tcp, fd, true);

        string keyword = response[0];
        string status = response[1];
        if (status == string("NOK")) {
          cout
              << "An error occurred while receiving state. Have you played any "
                 "game yet?"
              << endl
              << endl;
        } else if (status == string("FIN")) {
          cout << "The game has ended." << endl << endl;
        }

        close(fd);
      }
    } else if (command == string("quit")) {
      string message = string("QUT " + PLID);
      vector<string> response = send_message(message, res_udp, fd_udp, false);

      if (response[1] == string("OK")) {
        cout << "Ongoing game has been terminated successfully." << endl
             << endl;
      } else if (response[1] == string("NOK")) {
        cout << "No ongoing game exists." << endl << endl;
      } else {
        cout << "An unexpected error occurred." << endl << endl;
      }
    } else if (command == string("exit")) {
      string message = string("QUT " + PLID);
      vector<string> response = send_message(message, res_udp, fd_udp, false);
      cout << "Exiting..." << endl;
      break;
    } else {
      cout << "Unknown player command." << endl << endl;
    }
  }

  freeaddrinfo(res_udp);
  freeaddrinfo(res_tcp);
  close(fd_udp);

  return 0;
}
