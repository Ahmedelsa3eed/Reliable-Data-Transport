#include <iostream>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <thread>
#include <chrono>
#include <sys/types.h>
#include <sys/socket.h>

/* Data only packets */
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
const int PORT = 8080;
const int BUFFER_SIZE = 1024;

void writeFile(char *fileName, std::string content, size_t nBytes, size_t startLineSize)
{
    FILE *fp;
    fp = fopen(fileName, "wb");
    if (fp == NULL)
        return;
    fwrite(content.c_str() + startLineSize, sizeof(char), nBytes - startLineSize, fp);
    fclose(fp);
}

int main() {
    // Create a socket
    int client_fd = socket(AF_INET, SOCK_STREAM, 0);

    // Connect to the server
    sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &server_address.sin_addr);
    connect(client_fd, (sockaddr*) &server_address, sizeof(server_address));

    std::string request;
    std::cout << "Enter a message: ";
    std::getline(std::cin, request);
    send(client_fd, request.c_str(), request.length(), 0);
    std::cout << "Sent request to server" << std::endl;
    std::string response;
    while (true) {

        // Wait for an acknowledgement message from the server
        char buffer[BUFFER_SIZE];
        int bytes_received = recv(client_fd, buffer, BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            break;
        }
        std::cout << "Received " << bytes_received << " bytes: " << buffer << std::endl;
        // Send an acknowledgement message to the server
        response += std::string(buffer, bytes_received);
        std::string ack = "ACK";
        send(client_fd, ack.c_str(), ack.length(), 0);
    }

    writeFile("ClientTest.txt", response, response.length(), 0);

    // Close
    close(client_fd);
}