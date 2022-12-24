# Hangman - Computer Networks

## Compilation

Produce the required binaries by running:

``` 
make
```

## Running

### Server

Start the server by running the `GS` binary while providing the required list of words:

``` 
./GS word_eng.txt
```

### Client

Start the client by running the `player` binary:

``` 
./player
```

...and start playing!

Available player commands are:
* `start <player_id>`, where `<player_id>` must be a 6-digit number.
* `play <letter>` to guess a letter.
* `guess <word>` to guess the entire word.
* `hint` to receive a hint image.
* `quit` to quit the current game.
* `exit` to quit the current game and exit the client.

## Notes

The server expects the hint files (images hinting at the words to be guessed) to be under the `hints/` directory.

## Credits

Work done in pair-programming with [Mafalda Ribeiro](https://github.com/mafrib).
