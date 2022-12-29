#include <iostream>
#include <string>
#include <cstring>
#include <thread>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <vector>
#define MSS 16 // Maximum Segment Size
#define WINDOWSIZE 4

using namespace std;

const int TIMEOUT_SECONDS = 5;
const int PORT = 8080;
const int BUFFER_SIZE = 16;
vector<bool> ackedPackets(WINDOWSIZE, false);

typedef struct packet {
    /* Header */
    uint16_t chsum;
    uint16_t len;
    uint16_t seqno;
    /* Data */
    char data[MSS];
} packet;

typedef struct ack_packet {
    uint16_t chsum;
    uint16_t ackno;
    uint16_t len;
} ack_packet;

typedef struct MessageArgs {
    sockaddr_in client_address{};
    string filePath;
} MessageArgs;

typedef struct packetHandlerData {
    int sockfd;
    packet pck;
    sockaddr_in client_address{};
    int handlerId;
} PacketHandlerData;

packet make_packet(uint16_t seqno, uint16_t len, char data[]);
vector<packet> readFile(char *fileName);
int timeOut(int sockfd);
void handle_connection(void* args);
void sendDataChunks(int sockfd, sockaddr_in client_address, char *fileName);
void *sendOnePacket(void *arg);

int main() {
    // Create a socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        cerr << "Error creating socket" << endl;
        return 1;
    }

    // Bind the socket to the port
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    if (bind(sockfd, (sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        cerr << "Error binding socket to port" << endl;
        return 1;
    }

    while (true) {
        // Wait for a connection
        char buffer[BUFFER_SIZE];
        cout << "Server: Waiting for a connection..." << endl;
        sockaddr_in client_addr{};
        socklen_t client_addr_len = sizeof(client_addr);

        // Receive first datagram from the client
        long bytes_received= recvfrom(sockfd, buffer, BUFFER_SIZE, 0,
                    (sockaddr *) &client_addr, &client_addr_len);
        if (bytes_received < 0) {
            cerr << "Error accepting connection" << endl;
            continue;
        }

        // Create a new thread to handle the connection
        MessageArgs messageArgs;
        messageArgs.client_address = client_addr;
        string str = buffer;
        messageArgs.filePath = str.substr(0, bytes_received);
        pthread_t thread;
        pthread_create(&thread, nullptr, reinterpret_cast<void *(*)(void *)>(handle_connection), &messageArgs);
    }

}

void handle_connection(void* args) {
    // Get the socket and client address from the arguments
    int newSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (newSocket < 0) {
        cerr << "Error creating socket" << endl;
        return ;
    }

    auto* message_args = (MessageArgs*) args;
    sockaddr_in client_address = message_args->client_address;
    string filePath = message_args->filePath;
    // should handle the send of data in chunks
    sendDataChunks(newSocket, client_address, (char *)filePath.c_str());
    // Close the connection
    close(newSocket);
}

// Send the packets to the client
void sendDataChunks(int sockfd, sockaddr_in client_address, char *fileName) {
    vector<packet> packets = readFile(fileName);
    size_t n = packets.size();
    size_t send_base = 0;
    pthread_t windowThreads[WINDOWSIZE];

    while (true) {
        for (int i = send_base; i < send_base + WINDOWSIZE &&
            i < n && !ackedPackets[(i-send_base) % WINDOWSIZE]; i++)
        {
            int idx = (i-send_base) % WINDOWSIZE;
            PacketHandlerData threadData;
            threadData.sockfd = sockfd;
            threadData.pck = packets[i];
            threadData.client_address = client_address;
            threadData.handlerId = idx;

            pthread_create(&windowThreads[idx], NULL, sendOnePacket, &threadData);
            if (i == send_base + WINDOWSIZE) { // last iteraion
                pthread_join(windowThreads[send_base % WINDOWSIZE], NULL);
                while (send_base < n && ackedPackets[send_base % WINDOWSIZE]) {
                    ackedPackets[send_base % WINDOWSIZE] = false;
                    send_base++;
                }
            }
        }

        // base case
        if (send_base + 1 == n)
            break;
    }
}

// read the contents of a file into a vector of packet structs
vector<packet> readFile(char *fileName) {
    FILE *fp;
    vector<packet> packets;
    char *content = (char *)malloc(10000);
    fp = fopen(fileName, "rb");
    if (fp == nullptr)
        return packets;
    int nBytes = 0;
    int seqno = 0;
    while (fread(&content[nBytes], sizeof(char), 1, fp) == 1) {
        nBytes++;
        seqno++;
        if (nBytes == MSS) {
            packet p = make_packet(seqno, nBytes, content);
            packets.push_back(p);
            nBytes = 0;
        }
    }
    if (nBytes != 0) {
        packet p = make_packet(seqno, nBytes,content);
        packets.push_back(p);
    }
    fclose(fp);
    free(content);
    return packets;
}

packet make_packet(uint16_t seqno, uint16_t len, char data[]) {
    packet p;
    p.chsum = 0;
    p.len = len;
    p.seqno = seqno;
    strcpy(p.data, data);
    return p;
}

// wait for a specified amount of time for a socket to become readable
int timeOut(int sockfd) {
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(sockfd, &read_fds);
    // Set up the timeout
    struct timeval timeout{};
    timeout.tv_sec = TIMEOUT_SECONDS;
    timeout.tv_usec = 0;

    // Wait for the socket to become readable or for the timeout to expire
    int status = select(sockfd + 1, &read_fds, nullptr, nullptr, &timeout);

    return status;
}

void *sendOnePacket(void *arg) {
    PacketHandlerData *data = (PacketHandlerData *) arg;

    // wait acknowledgement from client
    int status = timeOut(data->sockfd);
    sendto(data->sockfd, &data->pck, sizeof(long)*3 + data->pck.len, 0,
           (sockaddr *)&data->client_address, sizeof(data->client_address));
    
    if (status == -1) {
        // An error occurred
        cerr << "Error waiting for socket: " << strerror(errno) << endl;
        return NULL;
    }
    else if (status == 0) {
        // The timeout expired
        cerr << "Timeout expired" << endl;
        sendto(data->sockfd, &data->pck, sizeof(long)*3 + data->pck.len, 0,
               (sockaddr *)&data->client_address, sizeof(data->client_address));
    }
    else {
        ack_packet ack;
        socklen_t client_addr_len = sizeof(data->client_address);
        ssize_t bytes_received = recvfrom(data->sockfd, &ack, sizeof(ack), 0,
                                       (sockaddr *)&data->client_address, &client_addr_len);
        if (bytes_received <= 0)
            return NULL;
        ackedPackets[data->handlerId] = true;
        cout << "Received " << bytes_received << " bytes: "
             << "with ackno: " << ack.ackno << endl;
    }
    return NULL;
}