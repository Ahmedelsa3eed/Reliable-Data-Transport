#include <iostream>
#include <time.h>
#include <string>
#include <cstdint>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <cinttypes>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <vector>
#include <fstream>
#include <string.h>
#define MSS 16 // Maximum Segment Size
#define MAXDATASIZE 1024 // 1k bytes: max number of bytes we can get at once

using namespace std;

typedef struct ack_packet {
    uint16_t chsum;
    uint16_t ackno;
    uint16_t len;
} ack_packet;

typedef struct packet {
    /* Header */
    uint16_t chsum;
    uint16_t len;
    uint16_t seqno;
    /* Data */
    char data[MSS];
} packet;

int sock_fd = 0, client_fd;
long byte_count = 0;
struct sockaddr_in serv_addr;
socklen_t fromlen;
vector<string> clientData(3); // <IP, port, filename>
packet *pck;
char buffer[MAXDATASIZE];

void readClientData();
void writeServerData();
void receiveServerData();

int main() {
    printf("Client: starting... \n");
    pck = (packet *) malloc(sizeof(packet));

    // Creating socket file discriptor
    if ((sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
        printf("\n Socket creation error \n");
        return -1;
    }
    
    memset(&serv_addr, 0, sizeof(serv_addr));

    readClientData();
    
    serv_addr.sin_family = AF_INET; // set to AF_INET to use IPv4
    serv_addr.sin_port = htons(stoi(clientData[1]));
    if (inet_pton(AF_INET, clientData[0].c_str(), &serv_addr.sin_addr) == -1) {
        printf("\n Invalid address/ Address not supported \n");
        return -1;
    }

    strcpy(pck->data, clientData[2].c_str());
    pck->len = clientData[2].size();

    // send the packet data directly for now
    if ((sendto(sock_fd, pck->data, pck->len, 0,
                (struct sockaddr *)&serv_addr, sizeof(serv_addr))) == -1) {
        printf("\n Error: sendto \n");
        return -1;
    }

    receiveServerData();

    free(pck);
    close(sock_fd);
}

void readClientData() {
    ifstream infile("client.in");
    string line;
    int i = 0;
    while (getline(infile, line))
        clientData[i++] = line;
}

void receiveServerData() {
    // open fileOutputStream for writing server data
    ofstream wf("serverReceivedData.out", ios::out | ios::binary);
    if (!wf) {
        cout << "Cannot open file!" << endl;
        return;
    }
    uint16_t recv_base = 0;
    while (true) {
        fromlen = sizeof serv_addr;
        packet packet;
        ack_packet ack;
        // receive data in buffer
        if ((byte_count = recvfrom(sock_fd, &packet, sizeof(packet) , 0,
                                (struct sockaddr *)&serv_addr, &fromlen)) < 1)
        {
            printf("revfrom finished \n");
            break;
        }
        printf("Received packet with seqno %d and length %d\n", packet.seqno, packet.len);
        if (packet.seqno <= recv_base) {
            // duplicate pck
            ack.ackno = recv_base + 1;
        } else {
            // write received data
            wf.write(packet.data, packet.len);
            wf.flush();
            ack.ackno = packet.seqno + 1;
            recv_base = packet.seqno;
        }
        // Send acknowledgement after receiving and consuming a data packet
        ack.len = 0;
        sleep(10);
        sendto(sock_fd, &ack, sizeof(ack), 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    }
    wf.close();
    if (!wf.good()) {
        cout << "Error occurred at writing time!" << endl;
        return;
    }
}