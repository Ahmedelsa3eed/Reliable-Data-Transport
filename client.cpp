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

using namespace std;

typedef struct packet
{
    /* Header */
    uint16_t chsum;
    uint16_t len;
    uint16_t seqno;
    /* Data */
    char data[50];
} packet;

int sock_fd = 0, client_fd;
struct sockaddr_in serv_addr;
vector<string> clientData(3); // <IP, port, filename>
packet *pck;

void readClientData();

int main() {
    printf("Client: starting... \n");
    pck = (packet *) malloc(sizeof(packet));

    if ((sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
        printf("\n Socket creation error \n");
        return -1;
    }

    readClientData();
    serv_addr.sin_family = AF_INET; // set to AF_INET to use IPv4
    serv_addr.sin_port = htons(stoi(clientData[1]));
    if (inet_pton(AF_INET, clientData[0].c_str(), &serv_addr.sin_addr) == -1)
    {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }

    strcpy(pck->data, clientData[2].c_str());

    if ((sendto(sock_fd, pck->data, pck->len, 0,
                (struct sockaddr *)&serv_addr, sizeof(serv_addr))) == -1)
    {
        perror("talker: sendto");
        return -1;
    }

    close(sock_fd);
}

void readClientData()
{
    ifstream infile("client.in");
    string line;
    int i = 0;
    while (getline(infile, line))
        clientData[i++] = line;
}