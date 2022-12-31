# Reliable Data Transport Protocol
This project is associated with Alexandria University.

## Inroduction
Implementing a service that guarantees the arrival of datagrams in the correct order on top of the UDP/IP protocol, along with congestion control.

## Features
### Stop and wait protocol
To run **stop-and-wait** without PLP: ``git checkout v1``  
To run **stop-and-wait** with PLP: ``git checkout PLP``
- packet loss simulation at server side
    - A datagram given as a parameter to the ``sendto()`` method is transmitted only ``100*(1 - PLP)%`` of the time
- client can detect out-of-order packets
    - it sends an ack for the last correctly received packet

### Selective repeat protocol
To run **selective-repeat** with **congestion control**: ``git checkout addingCongestionControl``

## Client workflow
- The client sends a datagram to the server to get a file giving its filename.
- Whenever a datagram arrives, an ACK is sent out by the client to the server.
- order the datagrams at the client side (in case of selective-repeat).

### Arguments for the client
- input file ``client.in`` from which it reads the following information, in the order shown, one item per line

        IP address of server
        Well-known port number of server
        Filename to be transferred (should be a large file)

## Server workflow
- The server creates a child thread to handle the client
- The server (child) creates a UDP socket to handle file transfer to the client
- Server sends its first datagram, the server uses some random number generator random() function to decide with probability p if the datagram would be passed to the method send() or just ignore sending it.
- If you choose to discard the package and not to send it from the server the timer will expire at the server waiting for the ACK that it will never come from the client (since the packet wasn’t sent to it) and the packet will be resent again from the server.

### Arguments for the server
- input file ``server.in`` from which it reads the following information, in the order shown, one item per line

      Well-known port number for server
      Random generator seed value
      Probability p of datagram loss (real number in the range [ 0.0 , 1.0]

### Congestion analysis
- Graph for the implemented TCP for the different values of PLP


PLP = 0.01 
![image](https://user-images.githubusercontent.com/60315006/210149622-0e7cc3a5-5213-4ee9-a886-48578b530cac.png)

PLP = 0.05
![image](https://user-images.githubusercontent.com/60315006/210149691-d1798e4e-1f92-45a1-af9c-8bad546ef307.png)

PLP = 0.1
![image](https://user-images.githubusercontent.com/60315006/210149728-0d6b3dc9-c0bd-412c-b094-c3aaadee592e.png)

PLP = 0.3
![image](https://user-images.githubusercontent.com/60315006/210149776-3e016302-0bc4-47ca-8425-e0745a0ce2c5.png)

