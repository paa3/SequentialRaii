/**
 * Simple UDP echo server on port 1234. Will return whatever is thrown to it.
 */

#include "../seqraii.h"
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <iostream>
#include <vector>

using namespace sequentialraii;

// Server UDP port.
constexpr unsigned short PORT = 1234;

// Receiving buffer max size.
constexpr int MAX_BUFFER_SIZE = 1024;

int main(int argc, char **argv)
{
    int socketfd;
    SequentialRaii udp;

    // Create socket.
    udp.addStep(
        [&]()
        {
            socketfd = socket(PF_INET, SOCK_DGRAM, 0);
            return (socketfd != -1);
        },
        [&]()
        {
            close(socketfd);
        });

    // Allow reuse of address.
    udp.addStep(
        [&]()
        {
            int optval = 1;
            return (setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, static_cast<void*>(&optval), sizeof(int)) !=-1);
        });

    // Construct server address and bind to the given port.
    udp.addStep(
        [&]()
        {
            sockaddr_in serveraddr = {};    // server address.
            serveraddr.sin_family = AF_INET;
            serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
            serveraddr.sin_port = htons(PORT);

            return (bind(socketfd, (sockaddr*)&serveraddr, sizeof(serveraddr)) == 0);
        });

    if (!udp.initialize())
    {
        return -1;
    }

    // Enter echo loop.
    while (true)
    {
        std::vector<char> buffer(MAX_BUFFER_SIZE, 0);

        // Read UDP datagram from the client.
        sockaddr_in clientaddr;
        socklen_t clientlen = sizeof(clientaddr);
        int bytesRead = recvfrom(socketfd, buffer.data(), MAX_BUFFER_SIZE, 0, (sockaddr*)&clientaddr, &clientlen);
        if (bytesRead < 0)
        {
            break;
        }

        // Print client ip:port <data>
        std::cout << inet_ntoa(clientaddr.sin_addr) << ":" << ntohs(clientaddr.sin_port) << " -> " << buffer.data() << std::endl;

        // Terminate on 'x' input.
        if (bytesRead == 1 and buffer[0] == 'x')
        {
            break;
        }

        // Return whatever was sent to us.
        int bytesSent = sendto(socketfd, buffer.data(), bytesRead, 0, (sockaddr*)&clientaddr, clientlen);
        if (bytesSent < 0)
        {
            break;
        }
    }

    // Clean up and leave.
    udp.uninitialize();

    return 0;
}

