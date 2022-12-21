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

// TODO timer udp, se houver tempo
// TODO terminal codes estão a aparecer nas mensagens; se houver tempo, garantir que as mensagens acabam sempre em \n
// TODO apagar debug no send_message
// TODO fazer/confirmar lógica do game_running; quando ganha/perde, ver resposta
// que o state deve dar se nunca houve jogos
// TODO ver se há hipóteses de mandar porcaria para o servidor (mensagens mal
// formadas)
// TODO protocolo é à risca, é tudo separado por um único espaço
// TODO ERR geral?
// TODO se houver tempo, ser resiliente a perder pacotes UDP
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
      exit(1); // TODO fazer outra coisa sem ser exit? Manter exit que são erros
               // importantes
    }

    // TODO ver se é preciso ver outras coisas (whitespace) -> não é
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
    string response_word = read_word(fd);
    if (response_word.length() == 0) {
      cout << "An error occured while trying to read from socket." << endl;
    } else {
      response.push_back(response_word);
      if (response_word == string("RSB") || response_word == string("RHL") ||
          response_word == string("RST")) {
        string status = read_word(fd);
        if (status.length() == 0) {
          cout << "An error occured while trying to read from socket." << endl;
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
              n = read(fd, tcp_buffer + bytes_read, tcp_buffer_size - bytes_read);
              if (n == -1) {
                printf("An error ocurred\n");
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
              cout << string(tcp_buffer);
            } else if (response_word == string("RHL")) {
              cout << "File name: " << Fname << ", file size: " << Fsize << endl
                   << endl;
            } else if (response_word == string("RST")) {
              cout << "File name: " << Fname << ", file size: " << Fsize << endl
                   << endl;
              cout << string(tcp_buffer) << endl;
            }

            free(tcp_buffer);
          } else {
            // TODO NOK?
          }
        }
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
  bool game_running = false;

  for (int i = 1; i < argc; i += 2) {
    if (strcmp(argv[i], "-n") == 0) {
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
  while (cout << "> " && getline(cin, input_line)) {
    // TODO check if any trash left after? passar para vector e ver
    // se tamanho é demasiado grande de acordo com cada comando

    stringstream line_stream(input_line);
    string command;
    line_stream >> command;

    if (command == string("start") || command == string("sg")) {
      line_stream >> PLID;
      if (PLID.length() != 6) {
        cout << "Invalid PLID provided. Please try again." << endl;
        exit(1); // TODO tirar exit, voltar a pedir
      }

      for (char digit : PLID) {
        if (!isdigit(digit)) {
          cout << "Invalid PLID provided. Please try again." << endl;
          exit(1); // TODO tirar exit, voltar a pedir
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
        game_running = true;

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
        cout << "An error occurred while creating socket." << endl;
      } else if (connect(fd, res_tcp->ai_addr, res_tcp->ai_addrlen) == -1) {
        cout << "An error occurred while connecting to socket." << endl;
      } else {
        string message = string("GSB");
        vector<string> response = send_message(message, res_tcp, fd, true);

        // TODO ver se RSB
        string keyword = response[0];
        string status = response[1];
        if (status == string("EMPTY")) {
          cout << "No game was yet won by any player." << endl << endl;
        }
      }
      close(fd);
      // TODO dá hang quando se faz hint sem ter um jogo iniciado
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

        // TODO ver se RHL
        string keyword = response[0];
        string status = response[1];
        if (status == string("NOK")) {
          cout << "An error occurred while receiving hint." << endl << endl;
        }
        close(fd);
      }
    } else if (command == string("state") ||
               command == string("st")) { // TODO state sem jogo iniciado também
                                          // dá segfault (read_word)
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
        string message = string("STA " + PLID);
        vector<string> response = send_message(message, res_tcp, fd, true);

        // TODO ver se RST
        string keyword = response[0];
        string status = response[1];
        if (status == string("NOK")) {
          cout << "An error occured while receiving state. Have you played any "
                  "game yet?"
               << endl
               << endl;
        }
        // TODO else if status FIN, acabar o jogo
        close(fd);
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

  return 0;
}
