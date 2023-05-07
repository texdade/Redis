#include <iostream>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>
#include <vector>
#include <poll.h>

const size_t MAX_REQUEST_SIZE = 4096;
enum {
    STATE_REQ = 0,
    STATE_RES = 1,
    STATE_END = 2,
};

struct Connection{
    int socket = -1;
    uint32_t state = STATE_REQ;
    size_t rBuffSize = 0;
    uint8_t rBuff[4 + MAX_REQUEST_SIZE];
    size_t wBuffSize = 0;
    size_t wBuffSent = 0;
    uint8_t wBuff[4 + MAX_REQUEST_SIZE];
};

void doStuff(int connection)
{
    char rBuf[64] = {};
    ssize_t n = read(connection, rBuf, sizeof(rBuf) - 1);
    if (n < 0) {
        return;
    }
    std::cout << "Received: " << rBuf << std::endl;

    char wbuf[] = "world";
    write(connection, wbuf, strlen(wbuf));
}

//read <size> bytes from socket <server> into <buff>
int32_t readSpecificSize(int server, char* buff, size_t size)
{
    while(size > 0)
    {
        ssize_t readSize = read(server, buff, size);
        if(readSize < 0)
        {
            return -1;
        }

        if((size_t)readSize > size)
        {
            return -1;
        }

        size -= (size_t)readSize;
        buff += readSize;
    }

    return 0;
}

//write <size> bytes into socket <server> from <buff>
int32_t writeSpecificSize(int server, char* buff, size_t size)
{
    while(size > 0)
    {
        ssize_t writeSize = write(server, buff, size);
        if(writeSize < 0)
        {
            return -1;
        }

        if((size_t)writeSize > size)
        {
            return -1;
        }

        size -= (size_t)writeSize;
        buff += writeSize;
    }

    return 0;
}

int32_t getRequest(int server)
{
    //4 is reserved for the header
    char rBuff[4 + MAX_REQUEST_SIZE + 1] = {};
    errno = 0;

    //read the header (length of message)
    int32_t err = readSpecificSize(server, rBuff, 4);
    if(err)
    {
        return -1;
    }

    uint32_t len = 0;
    memcpy(&len, rBuff, 4);
    if(len > MAX_REQUEST_SIZE)
    {
        return -1;
    }

    //read the message itself
    err = readSpecificSize(server, rBuff, len);
    if(err)
    {
        return -1;
    }

    rBuff[4 + len] = '\0';
    std::cout << "Received: " << rBuff << std::endl;

    const char reply[] = "Prova";
    char wBuff[4 + sizeof(reply)] = {};
    len = (uint32_t)sizeof(reply);
    memcpy(wBuff, &len, 4);
    memcpy(&wBuff[4], reply, len);
    return writeSpecificSize(server, wBuff, 4 + len);
}

void setNonBlockingServer(int server)
{
    //get current flags
    int flags = fcntl(server, F_GETFL, 0);
    if (flags == -1) {
        abort();
    }

    //set non blocking
    flags |= O_NONBLOCK;
    int s = fcntl(server, F_SETFL, flags);
    if (s == -1) {
        abort();
    }
}

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        abort();
    }

    //vector of all connections
    std::vector<Connection*> connections;

    //set non blocking
    setNonBlockingServer(sock);

    std::vector<struct pollfd> pollArgs;
    
    //event loop!
    while(true)
    {
        pollArgs.clear();
        pollArgs.push_back({sock, POLLIN, 0});

        for(Connection* connection : connections)
        {
            if(!connection)
            {
                continue;
            }

            //set up all the right flags based on status of the connection
            struct pollfd poll = {};
            poll.fd = connection->socket;
            poll.events = (connection->state == STATE_REQ) ? POLLIN : POLLOUT;
            poll.events = poll.events | POLLERR | POLLHUP;
            pollArgs.push_back(poll);
        }

        //poll for active connections
        int err = poll(pollArgs.data(), pollArgs.size(), -1);
        if(err < 0)
        {
            abort();
        }

        //process
        for(const pollfd& pfd : pollArgs)
        {
            if(pfd.revents)
            {
                Connection* connection = connections[pfd.fd];
                // TODO: handle io
                if(connection->state == STATE_END)
                {
                    connections[pfd.fd] = nullptr;
                    close(pfd.fd);
                    free(connection);
                }
            }
        }

        if(pollArgs[0].revents & POLLIN)
        {
            //TODO: accept new connection
        }
    }
}