CC=g++
DEPS=utils.h

all:
	make client
	make server

client: client.cpp
	$(CC) client.cpp -o client

server: server.cpp
	$(CC) server.cpp -o server