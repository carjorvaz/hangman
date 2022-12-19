#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <ctype.h>
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

// TODO fazer função que lê uma palavra e um espaço, de um socket TCP para usar
// no send_message
// TODO do scoreboard, praticamente só falta escrever o ficheiro
// TODO ao ler response, fazer:
// TODO fazer README
// TODO fazer autoavaliação e incluir no zip do projeto (junto com o README)
// string <keyword> = response[0];
// response.erase(response.begin());
// TODO ter um símbolo de prompt? Por exemplo: >
// TODO mostrar palavra quando se perde?
// TODO nix-shell compile shortcut
// TODO makefile (essencial é que compile o código, o resto é de pouca
// importância)
// TODO mudar os response[0], response[1], etc. para os nomes que aparecem no
// enunciado (n_letters e assim)
// TODO usar string.length() e ver se letra está a <= z e A <= Z
// TODO quando estou a detetar erros, fazer if, else if e finalmente else se
// estiver tudo bem (em vez de fazer exit(1), como tenho no start no PLID))
// TODO só fazer exit(1) com erros relacionados com argumentos de linha de
// comandos; em erros de comandos, pedir outra vez
// TODO copiar makefile de so?
// TODO fazer string message para todos os send message
// TODO buffer de tamanho maior para tcp?
// TODO não ter lista de fds tcp, abrir e fechar tcp no comando, simplesmente;
// ver se é preciso manter e falar sempre pelo mesmo tcp
// TODO para as imagens tcp, fazer mallac de n bytes (ver qual é o tipo)
// TODO fazer parte do TCP caracter a caracter
string read_word(int fd) {
  int n;
  char buffer_tcp[1];
  string word = "";
  while (true) {
    n = read(fd, buffer_tcp, 1);
    if (n == -1) {
      cout << "An error occurred." << endl;
      exit(1); // TODO fazer outra coisa sem ser exit? Manter exit que são erros importantes
    }

    // TODO ver se é preciso ver outras coisas (whitespace)
    if (buffer_tcp[0] == ' ') {
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
  vector<string> response;
  addrlen = sizeof(addr);

  char *msg = strdup((str_msg + "\n").c_str());
  n = sendto(fd, msg, strlen(msg), 0, res->ai_addr, res->ai_addrlen);
  if (n == -1) {
    cout << "An error occurred." << endl;
    free(msg);
    exit(1);
  }

  free(msg);

  if (!is_tcp) {
    n = recvfrom(fd, buffer, buffer_size, 0, (struct sockaddr *)&addr,
                 &addrlen);
    if (n == -1) {
      cout << "An error occurred." << endl;
      exit(1); // TODO fazer outra coisa sem ser exit?
    }

    string buffer_str = "";
    buffer_str += string(buffer);
    stringstream response_stream(buffer_str);
    string response_word;

    //
    cout << endl << "Message: " << str_msg << endl;
    //

    response_stream >> response_word;
    response.push_back(response_word);
    while (response_stream >> response_word) {
      response.push_back(response_word);
    }
  } else {
    // TODO encher o response?
    string response_word = read_word(fd);
    response.push_back(response_word);

    if (response_word == string("RSB")) {
      string status = read_word(fd);
      response.push_back(status);

      if (status == string("OK")) { // TODO: função que lê character a character
                                    // até ao espaço do socket tcp
        string Fname = read_word(fd);
        response.push_back(Fname);

        string Fsize = read_word(fd);
        response.push_back(Fsize);

        // TODO experimentar continuar a ler para o mesmo buffer; ter
        // preocupação de ler buffer size - 1 para o \0?
        int scoreboard_buffer_size = stoi(Fsize) * sizeof(char);
        char *scoreboard_buffer =
            (char *)malloc(scoreboard_buffer_size); // TODO free
        // getline(response_stream, remaining_buffer, '\0');

        while (true) {
          n = read(fd, scoreboard_buffer, scoreboard_buffer_size);
          if (n == -1) {
            printf("An error ocurred\n");
            exit(1);
          }

          else if (n == 0) {
            break;
          }
        }

        string Fdata(scoreboard_buffer);
        response.push_back(Fdata);
      } else {
        // TODO NOK?
      }
    }
  }

    //
    cout << "Response: ";
    for (string response_word : response) {
      cout << response_word << " ";
    }
    cout << endl << endl;
    //

  return response;
}

int main(int argc, char **argv) {
  int errcode;
  int fd_udp;
  vector<int> fds_tcp;
  struct addrinfo hints_udp, *res_udp;
  struct addrinfo hints_tcp, *res_tcp;

  int GN = 9;
  string GSIP = "0.0.0.0"; // IPv4
  string GSport = to_string(58000 + GN);

  string PLID;
  string word;
  int trial = 0;
  int errors = 0;
  int max_errors = 0;

  for (int i = 1; i < argc; i += 2) {
    if (strcmp(argv[i], "-n") == 0) {
      // TODO validate IPv4 with regex?
      // https://stackoverflow.com/questions/5284147/validating-ipv4-addresses-with-regexp
      // inet_pton?
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

  fd_udp = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd_udp == -1) {
    cout << "An error occurred." << endl;
    exit(1);
  }

  memset(&hints_udp, 0, sizeof hints_udp);
  hints_udp.ai_family = AF_INET;
  hints_udp.ai_socktype = SOCK_DGRAM;
  errcode = getaddrinfo(GSIP.c_str(), GSport.c_str(), &hints_udp, &res_udp);
  if (errcode != 0) {
    cout << "An error occurred." << endl; // TODO error message melhor?
    exit(1);
  }

  memset(&hints_tcp, 0, sizeof hints_tcp);
  hints_tcp.ai_family = AF_INET;
  hints_tcp.ai_socktype = SOCK_STREAM;
  errcode = getaddrinfo(GSIP.c_str(), GSport.c_str(), &hints_tcp, &res_tcp);
  if (errcode != 0) {
    cout << "An error occurred." << endl; // TODO error message melhor?
    exit(1);
  }

  string input_line, keyword;
  while (getline(cin, input_line)) {
    // TODO check if any trash left after? passar para vector e ver
    // se tamanho é demasiado grande de acordo com cada comando

    stringstream line_stream(input_line);
    string command;
    line_stream >> command;

    if (command == string("start") || command == string("sg")) {
      line_stream >> PLID;
      if (PLID.length() != 6) {
        cout << "Invalid PLID provided. Please try again." << endl;
        exit(1);
      }

      for (char digit : PLID) {
        if (!isdigit(digit)) {
          cout << "Invalid PLID provided. Please try again." << endl;
          exit(1);
        }
      }

      vector<string> response =
          send_message("SNG " + PLID, res_udp, fd_udp, false);
      string status = response[1];
      // TODO check if RSG? else error
      if (status == string("OK")) {
        int n_letters = stoi(response[2]);
        max_errors = stoi(response[3]);

        word = string(n_letters, '_');
        trial = 1;
        errors = 0;

        cout << "New game started! Guess " << n_letters << " letter word: ";
        for (int i = 0; i < n_letters; i++) {
          cout << "_";
        }
        cout << endl
             << "You can make up to " << max_errors << " errors." << endl
             << endl;
      } else {
        cout << "An error occurred. Is another game currently running?" << endl
             << endl;
      }
    } else if (command == string("play") || command == string("pl")) {
      string letter;
      line_stream >> letter;

      // TODO fazer estes checks com if else, etc. no resto (PLID e assim)
      // TODO fazer confirmações de isalpha,etc. no guess (se todas as letras
      // da palvra são letras, se a palavra tem o tamanho certo)
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

          // TODO check if RLG?

          // TODO print this information (better)
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
            for (int i = 0; i < word.length(); i++) {
              if (word[i] == '_')
                word[i] = letter[0];
            }

            cout << "You guessed it! The word is " << word << "!" << endl
                 << endl;
            // TODO end game (won)
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
                 << " is not part of the word. You have no guesses left. The "
                    "word "
                    "was "
                 << word << "." << endl // TODO this isn't the original word
                 << endl;
          } else if (status == string("INV")) {
            cout << "Invalid trial number. Contact your local client's "
                    "developer."
                 << endl
                 << endl;
          } else if (status == string("ERR")) {
            cout << "Error. Have you started the game yet?" << endl
                 << endl; // TODO mudar este print; várias vezes dá erro com o
                          // jogo já começado; ver se há outro print igual
          } else {
            cout << "Unexpected error." << endl << endl;
          }
        } else {
          // TODO no attempts left; show final word?; finish the game (lose)
          // TODO dá este erro se fizer jogada sem jogo começado, mudar
          cout << "You have no attempts left. Game over." << endl << endl;
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
          // TODO check if RWG?
          // TODO print this information (better)
          if (response[1] == string("WIN")) {
            word = guessed_word;
            cout << "You guessed it! The word is indeed " << word << "!" << endl
                 << endl;
            // TODO end game (won)
          } else if (response[1] == string("DUP")) {
            cout << "Duplicate guess, try again." << endl << endl;
          } else if (response[1] == string("NOK")) {
            trial++;
            errors++;

            cout << "Wrong guess. Try again." << endl << endl;
          } else if (response[1] == string("OVR")) {
            cout << "Wrong guess. You have no guesses left. The word was "
                 << word << "." << endl
                 << endl; // TODO isn't the original word
          } else if (response[1] == string("INV")) {
            cout << "Invalid trial number. Contact your local client's "
                    "developer."
                 << endl
                 << endl;
          } else if (response[1] == string("ERR")) {
            cout << "Error. Have you started the game yet?" << endl << endl;
            // TODO também pode dar erro se a guessed_word não tiver o mesmo
            // comprimento da palavra
          }
        } else {
          // TODO no attempts left; show final word?; finish the game (lose)
          cout << "You have no attempts left. Game over." << endl << endl;
        }
      }
    } else if (command == string("scoreboard") || command == string("sb")) {
      int fd = socket(AF_INET, SOCK_STREAM, 0);
      if (fd == -1) {
        cout << "An error occurred." << endl;
      } else if (connect(fd, res_tcp->ai_addr, res_tcp->ai_addrlen) == -1) {
        cout << "An error occurred." << endl;
      } else {
        string message = string("GSB");
        vector<string> response = send_message(message, res_tcp, fd, true);

        // TODO ver se RSB
        string keyword = response[0];
        response.erase(response.begin());
        string status = response[0];
        response.erase(response.begin());
        if (status == string("EMPTY")) {
          cout << "No game was yet won by any player." << endl << endl;
        } else if (status == string("OK")) {
          int n;

          string Fname = response[0];
          response.erase(response.begin());

          string Fsize = response[0];
          response.erase(response.begin());

          string Fdata = response[0];
          response.erase(response.begin());

          // TODO investigar porque é que este write às vezes se parte todo
          // n = write(1, Fdata.c_str(), stoi(Fsize));
          // if (n == -1) {
          //   cout << "An error occurred" << endl << endl;
          // }
          cout << Fdata.substr(
              0, stoi(Fsize)); // TODO fsize é bytes, vai correr mal; se
                               // calhar fazer mesmo este em modo C

          // TODO já sei, TCP não manda por ordem; corrigir lá em cima (como?)
        }

        // TODO close fd, apagar lista de fds_tcp (não existe esse conceito no
        // client, é abrir e fechar uma sessão TCP)

        fds_tcp.push_back(fd); // TODO apagar
      }
    } else if (command == string("quit")) {
      string message = string("QUT " + PLID);
      vector<string> response = send_message(message, res_udp, fd_udp, false);

      // TODO check if QUT (else ERR)?
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
      // exit(0); // TODO isto estava mal
      break;
    } else {
      cout << "Unknown player command." << endl << endl;
    }
  }

  freeaddrinfo(res_udp);
  freeaddrinfo(res_tcp);
  close(fd_udp);
  for (int fd_tcp : fds_tcp) {
    close(fd_tcp);
  }

  return 0;
}
