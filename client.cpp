#include "utils.h" // contains utilities, some shared with server.cpp

int main(int argc, char const *argv[])
{
    string server_ip;
    string server_port;
    if (argc == 2) {
        read_config(argv[1], server_ip, server_port);
    } else {
        cout << "Please pass in a client.conf when running this program,\n";
        cout << "i.e., ./client client.conf" << endl;
        return 0;
    }

    int socket_fd, num_bytes;
    struct addrinfo hints, *server_info;
    char buffer[MAX_DATA_SIZE];

    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_INET;
    
    if (getaddrinfo(server_ip.c_str(), server_port.c_str(), &hints, &server_info) != 0) {
        perror("getaddrinfo");
        return 1;
    }
    socket_fd = socket(server_info->ai_family, server_info->ai_socktype, server_info->ai_protocol);
    if (socket_fd == -1) {
        perror("socket");
    }
    if (connect(socket_fd, server_info->ai_addr, server_info->ai_addrlen) == -1) {
        close(socket_fd);
        perror("connect");
    }
    if (server_info == NULL) {
        fprintf(stderr, "Failed to connect\n");
        return 2;
    }
    freeaddrinfo(server_info);

    string command; 
    while (true) {
        num_bytes = recv(socket_fd, buffer, MAX_DATA_SIZE - 1, 0);
        if (num_bytes == -1) {
            perror("recv");
            exit(1);
        } else if (num_bytes == 0) {
            cout << "Server has closed the connection." << endl;
            break;
        }
        buffer[num_bytes] = '\0';
        cout << buffer << endl;
        
        string str(buffer);
        if (str.find("205 Connection closing") == string::npos) {
            getline(cin, command);
        }

        if (send(socket_fd, command.c_str(), sizeof(command), 0) == -1) {
            perror("send");
        }
    }

    close(socket_fd);
    return 0;
}
