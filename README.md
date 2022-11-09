# NNTP Client/Server Implementation

This is a basic client and server implementation for the NNTP protocol.

## Compilation

To compile, run the following command in the directory containing the source files client.cpp, server.cpp, and utils.h:

```
make
```

## Usage

First start the server with the following command:

```
./server server.conf
```

You can then connect to the server from a separate terminal by running the client with the following command:

```
./client client.conf
```

> Note the .conf files must supply the following values for the server:
> - NNTP_PORT
>
> For the client:
> - SERVER_IP
> - SERVER_PORT

Multiple clients can connect to the server concurrently. Once connected, you can issue commands from the client to receive responses from the server. Try the `HELP` command for a list of available commands.