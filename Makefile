##
# RC Word Game
#
CC=g++
CFLAGS=-Wall

CLIENT=player
SERVER=GS

all: $(CLIENT) $(SERVER)

$(CLIENT): $(CLIENT).cpp
	$(CC) $(CFLAGS) -o $(CLIENT) $(CLIENT).cpp

$(SERVER): $(SERVER).cpp
	$(CC) $(CFLAGS) -o $(SERVER) $(SERVER).cpp

clean:
	$(RM) $(CLIENT) $(SERVER)
