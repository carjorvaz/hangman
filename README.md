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

## Notes

The server expects the hint files (images hinting at the words to be guessed) to be under the `hints/` directory.
