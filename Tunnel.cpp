#include "Tunnel.hpp"
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if_tun.h>
#include <linux/if.h>
#include <fcntl.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <poll.h>
#include <stdexcept>
#include <cstdint>
#include <arpa/inet.h>

static int init_tunnel(const std::string& dev, const std::string& local, const std::string& remote)
{
    fprintf(stderr, "Setting up tunnel %s %s -> %s\n", dev.c_str(), local.c_str(), remote.c_str());
    int fd,sock;
    CHECK(fd=open("/dev/net/tun",O_RDWR));
    ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TUN;
    strncpy(ifr.ifr_name,dev.c_str(),IFNAMSIZ);
    CHECK(ioctl(fd, TUNSETIFF, &ifr));
    CHECK(fcntl(fd, F_SETFL, O_NONBLOCK));
    CHECK(sock=socket(PF_INET, SOCK_DGRAM, IPPROTO_IP));
    sockaddr_in addr={AF_INET, 0, inet_addr(local.c_str())};
    memcpy(&ifr.ifr_addr, &addr, sizeof(addr));
    CHECK(ioctl(sock, SIOCSIFADDR, &ifr));
    addr={AF_INET, 0, inet_addr(remote.c_str())};
    memcpy(&ifr.ifr_addr, &addr, sizeof(addr));
    CHECK(ioctl(sock, SIOCSIFDSTADDR, &ifr));
    close(sock);
    return fd;
}

void Tunnel::init(const char *data, size_t len)
{
    const char *name=data;
    const char *src=name+strlen(name)+1;
    const char *dst=src+strlen(src)+1;
    tunnel=init_tunnel(name, src, dst);
}

void Tunnel::initServer(const char *data, size_t len)
{
    const char *name=data;
    tunnel=init_tunnel(name, "172.16.0.1", "172.16.0.2");
    char msg[]="srv\x00""172.16.0.2\x00""172.16.0.1";
    send(1, msg, sizeof(msg));
}

void Tunnel::deliver(const char *data, size_t len)
{
    ssize_t n;
    while(len>0)
    {
        n=write(tunnel, data, len);
        fprintf(stderr, "Delivered %ld/%ld\n", n, len);
        if(n<0)
            throw LibcError("write");
        data+=n;
        len-=n;
    }
}

void Tunnel::send(char type, const char *data, uint16_t len)
{
    fprintf(stderr,"Sending %d bytes\n", len);
    char buf[3];
    buf[0]=type;
    uint16_t x=htons(len);
    memcpy(buf+1, &x, sizeof(x));
    ssize_t n=write(localOut, buf, 3);
    if(n!=3)
        throw LibcError("write1");
    do
    {
        CHECK(n=write(localOut, data, len));
        fprintf(stderr,"Sent %d/%d bytes\n", n, len);
        data+=n;
        len-=n;
    }
    while(len>0);
}

void Tunnel::process(char type, const char *data, size_t len)
{
    switch(type)
    {
        case 0:
            deliver(data, len);
            break;
        case 1:
            init(data,len);
            break;
        case 2:
            initServer(data,len);
            break;
        default:
            fprintf(stderr,"Ignoring packet of type %d with length %ld\n", type, len);
            break;
    }
}

void Tunnel::handshake()
{
    if(!server)
    {
        char data[]="client";
        send(2, data, sizeof(data));
    }
}

void Tunnel::work()
{
    handshake();
    for(;;)
    {
        pollfd fds[]={
            {tunnel,POLLIN|POLLHUP|POLLERR,0},
            {localIn,POLLIN,0},
            {localOut,POLLHUP|POLLERR,0},
            {-1,0,0}
        };
        int n=poll(fds, sizeof(fds)/sizeof(fds[0]), -1);
        if(n<0)
        {
            if(errno==EINTR)
                continue;
            else
                throw LibcError("poll");
        }
        for(pollfd *i=fds;i->fd>=0;i++)
            if(i->revents&(POLLHUP|POLLERR))
                return;
        if(fds[0].revents&POLLIN)	// pakiet z lokalnego systemu, opakowac i wyekspediowac
        {
            tunBuffer.read(tunnel);
            send(0, tunBuffer.data(), tunBuffer.length());
            tunBuffer.remove(tunBuffer.length());
        }
        if(fds[1].revents&POLLIN)	// zdalny pakiet, przetworzyc i wykonac albo dostarczyc
        {
            buffer.read(localIn);
            fprintf(stderr,"len=%ld=0x%lx\n", buffer.length(), buffer.length());
            while(buffer.length()>=3)
            {
                size_t len=ntohs(*reinterpret_cast<const uint16_t*>(buffer.data()+1));
                fprintf(stderr, "Packet legnth %ld=0x%lx, buffer length %ld=0x%lx\n", len, len, buffer.length(), buffer.length());
                size_t whole=len+3;
                if(buffer.length()>=whole)
                {
                    process(buffer.data()[0], buffer.data()+3, len);
                    buffer.remove(whole);
                }
                else
                    break;
            }
        }
    }
}
