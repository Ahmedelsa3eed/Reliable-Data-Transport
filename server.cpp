#include <iostream>
#include <string>
#include <cstring>
#include <thread>
#include <chrono>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

const int PORT = 8090;
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

typedef struct ack_packet {
    long chsum;
    long len;
    long ackno;
} ack_packet;

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
void sendDataChunks(int client_fd) {
    // Send the message to the client
    std::string  message = readFile("test.txt");

    for (int i = 0; i< message.length() ; i += BUFFER_SIZE) {
        if ( i + BUFFER_SIZE > message.length()) {
            send(client_fd, message.c_str() + i, message.length() - i, 0);
        }
        else {
            send(client_fd, message.c_str() + i, BUFFER_SIZE, 0);
        }

        // wait acknowledgement from client
        char buffer[BUFFER_SIZE];
        int bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            break;
        }
        std::cout << "Received " << bytes_received << " bytes: " << buffer << std::endl;


    }
}

void handle_connection(int client_fd) {
    // Receive a message from the client
    char buffer[BUFFER_SIZE];
    int bytes_received  = recv(client_fd, buffer, BUFFER_SIZE, 0);
    if (bytes_received <= 0) {
        return;
    }
    std::cout << "Received First request " << bytes_received << " bytes: " << buffer << std::endl;
    // should handle the send of data in chunks
    sendDataChunks(client_fd);

    // Close the connection
    close(client_fd);
}

int main() {
    // Create a socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
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

    // Listen for incoming connections
    if (listen(sockfd, BACKLOG) < 0) {
        std::cerr << "Error listening for connections" << std::endl;
        return 1;
    }

    while (true) {
        // Wait for a connection
        std::cout << "Waiting for a connection..." << std::endl;
        sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int client_fd = accept(sockfd, (sockaddr*)&client_addr, &client_addr_len);
        if (client_fd < 0) {
            std::cerr << "Error accepting connection" << std::endl;
            continue;
        }

        // Create a new thread to handle the connection
        std::thread t(handle_connection, client_fd);
        t.detach();
    }

    // Close the socket
    close(sockfd);

    return 0;
}
