#include <iostream>
#include <string>
#include <cstring>
#include <thread>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <vector>
using namespace std;

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
    char data[16];
}packet;

typedef struct ack_packet {
    long chsum;
    long len;
    long ackno;
} ack_packet;

typedef struct MessageArgs {
    sockaddr_in client_address;
    std::string filePath;
}MessageArgs;



packet make_packet(long seqno, int len,char data[]) {
    packet p;
    p.chsum = 0;
    p.len = len;
    p.seqno = seqno;
    strcpy(p.data, data);
    return p;
}
std::vector<packet> readFile(char *fileName)
{
    FILE *fp;
    std::vector<packet> packets;
    char *content = (char *)malloc(10000);
    fp = fopen(fileName, "rb");
    if (fp == NULL)
        return packets;
    int nBytes = 0;
    while (fread(&content[nBytes], sizeof(char), 1, fp) == 1) {
        nBytes++;
        if(nBytes == 16) {
            packet p = make_packet(nBytes, nBytes,content);
            packets.push_back(p);
            nBytes = 0;
        }
    }
    if (nBytes != 0) {
        packet p = make_packet(nBytes, nBytes,content);
        packets.push_back(p);
    }
    fclose(fp);
    free(content);
    return packets;
}
void sendDataChunks(int sockfd, sockaddr_in client_address , char *fileName) {
    // Send the message to the client
    std::vector<packet> packets = readFile(fileName);
    int n = packets.size();
    for (int i = 0; i< n ; i ++) {
        sendto(sockfd, &packets[i], sizeof(long)*3+packets[i].len, 0,
                   (sockaddr*) &client_address, sizeof(client_address));

        // wait acknowledgement from client
        char buffer[BUFFER_SIZE];
        socklen_t client_addr_len = sizeof(client_address);
        long bytes_received = recvfrom(sockfd, buffer, BUFFER_SIZE, MSG_WAITALL, (sockaddr *) &client_address, &client_addr_len);
        if (bytes_received <= 0) {
            break;
        }
        std::cout << "Received " << bytes_received << " bytes: " << buffer << std::endl;


    }
}

void* handle_connection(void* args) {
    // Get the socket and client address from the arguments
    int newSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (newSocket < 0) {
        std::cerr << "Error creating socket" << std::endl;
        return NULL;
    }
    MessageArgs* message_args = (MessageArgs*) args;
    sockaddr_in client_address = message_args->client_address;
    std::string filePath = message_args->filePath;
    // should handle the send of data in chunks
    sendDataChunks(newSocket, client_address , (char *)filePath.c_str());

    // Close the connection
    close(newSocket);
    return NULL;
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
        long bytes_received= recvfrom(sockfd, buffer, BUFFER_SIZE,  0, (sockaddr *) &client_addr, &client_addr_len);
        if (bytes_received < 0) {
            std::cerr << "Error accepting connection" << std::endl;
            continue;
        }

        // Create a new thread to handle the connection
        MessageArgs messageArgs;
        messageArgs.client_address = client_addr;
        std::string str;
        str = buffer;
        messageArgs.filePath = str.substr(0,bytes_received);
        pthread_t thread;
        pthread_create(&thread, NULL, handle_connection, &messageArgs);
    }

    close(sockfd);

    return 0;
}
