#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <string.h>

#include <iostream>
#include <sstream>
#include <vector>
using namespace std;

/*
 * NOTE:
 * - When debugging with wireshark, use the filter: ip.addr == <tejo's ip>
 * - Test with:
 *   ❯ g++ player.cpp -o player && ./player -n tejo.tecnico.ulisboa.pt -p 58011
 *
 * Known errors:
 * - Se cancelarmos um jogo com ^C, não o conseguimos parar na próxima execução.
 * É preciso parar manualmente com o nc.
 * Se houver tempo, fazer o comando exit quando se faz ^C.
 */

// TODO nix-shell compile shortcut
// TODO makefile
// TODO correr valgrind e ver se há frees por fazer
string send_message(string str_msg, struct addrinfo *res, int fd) {
  ssize_t n;
  socklen_t addrlen;
  struct sockaddr_in addr;
  char buffer[128];

  char *msg = strdup((str_msg + "\n").c_str());
  n = sendto(fd, msg, strlen(msg), 0, res->ai_addr, res->ai_addrlen);
  if (n == -1) {
    cout << "An error occurred." << endl;
    free(msg);
    exit(1);
  }

  free(msg);

  addrlen = sizeof(addr);
  n = recvfrom(fd, buffer, 128, 0, (struct sockaddr *)&addr, &addrlen);
  if (n == -1) {
    cout << "An error occurred." << endl;
    exit(1);
  }

  return string(buffer); // TODO double-check if this is okay
}

int main(int argc, char **argv) {
  int fd, errcode;
  struct addrinfo hints, *res;

  int GN = 9;
  string GSIP = "0.0.0.0"; // IPv4
  string GSport = to_string(58000 + GN);

  string PLID;
  int trial = 0;
  int errors = 0;
  int max_errors = 0;
  string word;

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
    cout << "An error occurred." << endl;
    exit(1);
  }

  memset(&hints, 0, sizeof hints);

  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;

  errcode = getaddrinfo(GSIP.c_str(), GSport.c_str(), &hints, &res);
  if (errcode != 0) {
    cout << "An error occurred." << endl;
    exit(1);
  }

  string input, keyword;
  while (getline(cin, input)) {
    stringstream line(input); // TODO check if any trash left after?
    line >> keyword;

    if (keyword == string("start") || keyword == string("sg")) {
      line >> PLID;
      if (PLID.length() != 6) {
        cout << "Invalid PLID provided." << endl;
        exit(1);
      }

      for (char digit : PLID) {
        if (!('0' <= digit <= '9')) {
          cout << "Invalid PLID provided." << endl;
          exit(1);
        }
      }

      trial = 1;
      stringstream response(send_message("SNG " + PLID, res, fd));

      string response_component;
      response >> response_component; // TODO check if RSG?
      response >> response_component;
      if (response_component == string("OK")) {
        response >> response_component;
        int length = stoi(response_component);
        word = string(length, '_');
        errors = 0;

        if (length <= 6) {
          max_errors = 7;
        } else if (length >= 7 && length <= 10) {
          max_errors = 8;
        } else {
          max_errors = 9;
        }

        cout << "New game started! Guess " << response_component
             << " letter word: ";
        for (int i = 0; i < length; i++) {
          cout << "_";
        }
        cout << endl << "You can make up to " << max_errors << " errors." << endl
             << endl;
      } else {
        cout << "An error occurred. Is another game currently running?" << endl
             << endl;
      }
    } else if (keyword == string("play") || keyword == string("pl")) {
      string letter;
      line >> letter;

      if (max_errors - errors > 0) {
        string message =
            string("PLG " + PLID + " " + letter + " " + to_string(trial));
        stringstream response(send_message(message, res, fd));

        string response_component;
        response >> response_component; // TODO check if RLG?
        response >> response_component;

        // TODO print this information (better)
        if (response_component == string("OK")) {
          response >> response_component;
          response >> response_component;
          int n = stoi(response_component);
          for (int i = 0; i < n; i++) {
            response >> response_component;
            int position = stoi(response_component);
            word[position - 1] = letter[0];
          }

          trial++;

          cout << "Correct guess! Current word: " << word << endl << endl;
        } else if (response_component == string("WIN")) {
          for (int i = 0; i < word.length(); i++) {
            if (word[i] == '_')
              word[i] = letter[0];
          }

          cout << "You guessed it! The word is " << word << "!" << endl << endl;
          // TODO end game (won)
        } else if (response_component == string("DUP")) {
          cout << "Duplicate guess, try again." << endl << endl;
        } else if (response_component == string("NOK")) {
          trial++;
          errors++;

          cout << "The letter " << letter << " is not part of the word." << endl
               << endl;
        } else if (response_component == string("OVR")) {
          cout
              << "The letter " << letter
              << " is not part of the word. You have no guesses left. The word "
                 "was "
              << word << "." << endl // TODO this isn't the original word
              << endl;
        } else if (response_component == string("INV")) {
          cout << "Invalid trial number. Contact your local client's developer."
               << endl
               << endl;
        } else if (response_component == string("ERR")) {
          cout << "Error. Have you started the game yet?" << endl << endl;
        } else {
          cout << "Unexpected error." << endl << endl;
        }
      } else {
        // TODO no attempts left; show final word?; finish the game (lose)
        cout << "You have no attempts left. Game over." << endl << endl;
      }
    } else if (keyword == string("guess") || keyword == string("gw")) {
      string guessed_word;
      line >> guessed_word;

      if (max_errors - errors > 0) {
        string message =
            string("PWG " + PLID + " " + guessed_word + " " + to_string(trial));
        stringstream response(send_message(message, res, fd));

        string response_component;
        response >> response_component; // TODO check if RWG?
        response >> response_component;

        // TODO print this information (better)
        if (response_component == string("WIN")) {
          word = guessed_word;
          cout << "You guessed it! The word is indeed " << word << "!" << endl
               << endl;
          // TODO end game (won)
        } else if (response_component == string("DUP")) {
          cout << "Duplicate guess, try again." << endl << endl;
        } else if (response_component == string("NOK")) {
          trial++;
          errors++;

          cout << "Wrong guess. Try again." << endl << endl;
        } else if (response_component == string("OVR")) {
          cout << "Wrong guess. You have no guesses left. The word was " << word
               << "." << endl
               << endl; // TODO isn't the original word
        } else if (response_component == string("INV")) {
          cout << "Invalid trial number. Contact your local client's developer."
               << endl
               << endl;
        } else if (response_component == string("ERR")) {
          cout << "Error. Have you started the game yet?" << endl << endl;
          // TODO também pode dar erro se a guessed_word não tiver o mesmo
          // comprimento da palavra
        }
      } else {
        // TODO no attempts left; show final word?; finish the game (lose)
        cout << "You have no attempts left. Game over." << endl << endl;
      }
    } else if (keyword == string("scoreboard") || keyword == string("sb")) {

    } else if (keyword == string("quit")) {
      string message = string("QUT " + PLID);
      stringstream response(send_message(message, res, fd));

      string response_component;
      response >> response_component; // TODO check if QUT (else ERR)?
      response >> response_component;

      if (response_component == string("OK")) {
        cout << "Ongoing game has been terminated successfully." << endl
             << endl;
      } else if (response_component == string("NOK")) {
        cout << "No ongoing game exists." << endl << endl;
      } else {
        cout << "An unexpected error occurred." << endl << endl;
      }
    } else if (keyword == string("exit")) {
      string message = string("QUT " + PLID);
      stringstream response(send_message(message, res, fd));
      cout << "Exiting..." << endl;
      exit(0);
    } else {
      cout << "Unknown player command." << endl << endl;
    }
  }

  freeaddrinfo(res);
  close(fd);

  return 0;
}
