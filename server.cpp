#include <iostream>
#include <string>
#include <cstring>
#include <thread>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

const int PORT = 8080;
const int BACKLOG = 5;
const int BUFFER_SIZE = 16;
const int TIMEOUT = 5;  // timeout in seconds
typedef struct packet {
    /* Header */
    long chsum;
    long len;
    long seqno;
    /* Data */
    char data[500];
}packet;

typedef struct MessageArgs {
    int socket_fd;
    sockaddr_in client_address;
    std::string filePath;
}MessageArgs;

typedef struct ack_packet {
    long chsum;
    long len;
    long ackno;
} ack_packet;

packet make_packet(long seqno, const std::string data) {
    packet p;
    p.chsum = 0;
    p.len = data.length();
    p.seqno = seqno;
    strcpy(p.data, data.c_str());
    return p;
}
char *readFile(char *fileName)
{
    FILE *fp;
    char *content = (char *)malloc(10000);
    fp = fopen(fileName, "rb");
    if (fp == NULL)
        return NULL;
    unsigned int nBytes = 0;
    while (fread(&content[nBytes], sizeof(char), 1, fp) == 1) {
        nBytes++;
    }
    fclose(fp);
    return content;
}
void sendDataChunks(int client_fd, sockaddr_in client_address , char *fileName) {
    // Send the message to the client
    std::string  message = readFile(fileName);

    for (int i = 0; i< message.length() ; i += BUFFER_SIZE) {
        if ( i + BUFFER_SIZE > message.length()) {
            sendto(client_fd, message.c_str() + i, message.length() - i, 0,
                   (sockaddr*) &client_address, sizeof(client_address));
        }
        else {
            sendto(client_fd, message.c_str() + i, BUFFER_SIZE, 0,
                   (sockaddr*) &client_address, sizeof(client_address));
        }

        // wait acknowledgement from client
        char buffer[BUFFER_SIZE];
        long bytes_received = recvfrom(client_fd, buffer, BUFFER_SIZE, 0,
                                      (sockaddr*) &client_address, (socklen_t*) sizeof(client_address));
        if (bytes_received <= 0) {
            break;
        }
        std::cout << "Received " << bytes_received << " bytes: " << buffer << std::endl;


    }
}

void* handle_connection(void* args) {
    // Get the socket and client address from the arguments
    MessageArgs* message_args = (MessageArgs*) args;
    int client_fd = message_args->socket_fd;
    sockaddr_in client_address = message_args->client_address;
    std::string filePath = message_args->filePath;
    char buffer[BUFFER_SIZE];
    socklen_t client_len = sizeof(client_address);

    // should handle the send of data in chunks
    sendDataChunks(client_fd, client_address , (char *)filePath.c_str());

    // Close the connection
    close(client_fd);
}

int main() {
    // Create a socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        std::cerr << "Error creating socket" << std::endl;
        return 1;
    }

    // Bind the socket to the port
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    if (bind(sockfd, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Error binding socket to port" << std::endl;
        return 1;
    }
    while (true) {
        // Wait for a connection
        char buffer[BUFFER_SIZE];
        std::cout << "Waiting for a connection..." << std::endl;
        sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        long bytes_received= recvfrom(sockfd, buffer, BUFFER_SIZE, 0, (sockaddr *) &client_addr, &client_addr_len);
        if (bytes_received < 0) {
            std::cerr << "Error accepting connection" << std::endl;
            continue;
        }

        // Create a new thread to handle the connection
        MessageArgs messageArgs;
        messageArgs.socket_fd = sockfd;
        messageArgs.client_address = client_addr;
        // get the file path from the buffer
        std::string str;
        str = buffer;
        messageArgs.filePath = str.substr(0,bytes_received);
        pthread_t thread;
        pthread_create(&thread, NULL, handle_connection, &messageArgs);
    }

    // Close the socket
    close(sockfd);

    return 0;
}
