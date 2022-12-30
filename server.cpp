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
pthread_mutex_t lock;
const int TIMEOUT_SECONDS = 0;
const int TIMEOUT_MICROSECONDS = 2*1000;
double PLP = 0.99; // Packet Loss Probability

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


typedef struct timeOutCalc {
    clock_t start;
    int loc;
    int maxTime;
    int new_fd;
} timeOutCalc;

void *calculateTimeOut(void *arg);
packet make_packet(uint16_t seqno, uint16_t len, char data[]);
vector<packet> readFile(char *fileName);
int timeOut(int sockfd);
void handle_connection(void* args);
void sendDataChunks(sockaddr_in client_address, char *fileName);
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
    auto* message_args = (MessageArgs*) args;
    sockaddr_in client_address = message_args->client_address;
    string filePath = message_args->filePath;
    // should handle the send of data in chunks
    sendDataChunks( client_address, (char *)filePath.c_str());
    // Close the connection
}

// Send the packets to the client
void sendDataChunks( sockaddr_in client_address, char *fileName) {
    vector<packet> packets = readFile(fileName);
    size_t n = packets.size();
    int send_base = 0;
    pthread_t windowThreads[WINDOWSIZE];

    for (int i = send_base; i < n ; i++)
        {
            int idx = (i-send_base) % WINDOWSIZE;
            auto *threadData  = (PacketHandlerData*) malloc(sizeof(PacketHandlerData));
            threadData->sockfd = socket(AF_INET, SOCK_DGRAM, 0);
            threadData->pck = packets[i];
            threadData->client_address = client_address;
            threadData->handlerId = idx;

            pthread_create(&windowThreads[idx], nullptr, sendOnePacket, threadData);
            if (i == send_base + WINDOWSIZE - 1) { // last iteraion
                cout<< "Waiting for acks of thread "<< send_base % WINDOWSIZE << endl;
                pthread_join(windowThreads[send_base % WINDOWSIZE], nullptr);
                while ( send_base < n && ackedPackets[ send_base % WINDOWSIZE ]) {
                    pthread_mutex_lock(&lock);
                    ackedPackets[send_base % WINDOWSIZE] = false;
                    pthread_mutex_unlock(&lock);
                    send_base++;
                }
            }
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
        if (nBytes == MSS) {
            packet p = make_packet(seqno, nBytes, content);
            seqno+=nBytes;
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
    timeout.tv_usec = TIMEOUT_MICROSECONDS;

    // Wait for the socket to become readable or for the timeout to expire
    int status = select(sockfd + 1, &read_fds, nullptr, nullptr, &timeout);

    return status;
}

void *sendOnePacket(void *arg) {

    auto *data = (PacketHandlerData *) arg;

    if ( (double)( rand() % 100 ) / 100.0 < PLP) {
        int status = timeOut(data->sockfd);
        if (status == 0) {
            printf("Timeout for packet %d \n", data->pck.seqno);
            return sendOnePacket(data);
        } else if (status == -1) {
            cout << " Select error" << endl;
        }
    } else {
        // wait acknowledgement from client
        sendto(data->sockfd, &data->pck, sizeof(long) * 3 + data->pck.len, 0,
               (sockaddr *) &data->client_address, sizeof(data->client_address));
        int status = timeOut(data->sockfd);
        if (status == 0) {
            cout << "Timeout for packet " << data->pck.seqno <<"Packet sent without receiving ACK "<<endl;
        } else if (status == -1) {
            cout << " Select error" << endl;
        } else {
            ack_packet ack;
            socklen_t client_addr_len = sizeof(data->client_address);
            ssize_t bytes_received = recvfrom(data->sockfd, &ack, sizeof(ack), 0,
                                              (sockaddr *) &data->client_address, &client_addr_len);
            if (bytes_received <= 0)
                return nullptr;
            pthread_mutex_lock(&lock);
            ackedPackets[data->handlerId] = true;
            pthread_mutex_unlock(&lock);

            cout << "Received " << bytes_received << " bytes: " << ack.ackno << endl;
            free(data);
        }
    }
    return nullptr;
}

void *calculateTimeOut(void *arg)
{
    timeOutCalc *timeOut = (timeOutCalc *)arg;
    clock_t start = timeOut->start;

    while ((double)((clock_t)clock() - timeOut->start) / CLOCKS_PER_SEC < timeOut->maxTime)
    {    /// wait for x seconds to get the request from the client

        if (timeOut->loc == 1) { /// if the request is received
            printf("[+] server receive request from client %d\n", timeOut->new_fd);
            while (timeOut->loc == 1) {
                /// wait for the client to send the request
            }
            /// start get updated in the client thread and the same for the loc
        }
    }

    printf("[+] Waited for %f seconds, close the connection with %d\n", (double)(clock() - start) / CLOCKS_PER_SEC, timeOut->new_fd);
    timeOut->start = -1;

    pthread_exit(NULL); /// exit the thread
}
