#include <iostream>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/ip.h>

const size_t MAX_REQUEST_SIZE = 4096;

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

int main() {
    int server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0) {
        abort();
    }
    int  val = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    struct sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(1234);
    serverAddr.sin_addr.s_addr = ntohl(0);

    int bindC = bind(server, (struct sockaddr*)&serverAddr, sizeof(serverAddr));

    if(bindC)
    {
        abort();
    }

    //start listening
    bindC = listen(server, SOMAXCONN);
    if(bindC)
    {
        abort();
    }

    while(true)
    {
        struct sockaddr_in clientAddr = {};
        socklen_t clientAddrSize = sizeof(clientAddr);
        int connection = accept(server, (struct sockaddr*)&clientAddr, &clientAddrSize);
        if(connection < 0)
        {
            //current connection is broken, start listening again
            continue;
        }

        while(true)
        {
            int32_t status = getRequest(connection);
            if(status)
            {
                break;
            }
        }

        doStuff(connection);
        close(connection);
    }
}