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

const char SEP='\x01';

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

void Tunnel::init(char *data, size_t len)
{
    data[len-1]=0;
    char *ptrs[3];
    ptrs[0]=data;
    for(int i=1;i<3;++i)
    {
        ptrs[i]=strchr(ptrs[i-1], SEP);
        if(ptrs[i]==NULL)
            return;
        *ptrs[i]='\0';
        ptrs[i]++;
    }
    tunnel=init_tunnel(ptrs[0], ptrs[1], ptrs[2]);
}

void Tunnel::initServer(const char *name, size_t len)
{
    if(name[len-1]!=0)
        return;
    std::string remoteIp=cfg.ip(name);
    if(remoteIp.empty())
    {
        throw std::runtime_error(std::string("Unkown client '")+name+"'");
        return;
    }
    std::string msg=cfg.name()+SEP+remoteIp+SEP+cfg.ip();
    send(MessageType::DHCP, msg);
    tunnel=init_tunnel(name, cfg.ip(), remoteIp);
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

void Tunnel::send(MessageType type, const char *data, uint16_t len)
{
    fprintf(stderr,"Sending %d bytes\n", len);
    char buf[3];
    buf[0]=static_cast<char>(type);
    uint16_t x=htons(len);
    memcpy(buf+1, &x, sizeof(x));
    ssize_t n=write(localOut, buf, 3);
    if(n!=3)
        throw LibcError("write1");
    do
    {
        CHECK(n=write(localOut, data, len));
        fprintf(stderr,"Sent %ld/%d bytes\n", n, len);
        data+=n;
        len-=n;
    }
    while(len>0);
}

void Tunnel::process(MessageType type, char *data, size_t len)
{
    switch(type)
    {
        case MessageType::PACKET:
            deliver(data, len);
            break;
        case MessageType::DHCP:
            init(data,len);
            break;
        case MessageType::HANDSHAKE:
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
        send(MessageType::HANDSHAKE, cfg.name());
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
            send(MessageType::PACKET, tunBuffer.data(), tunBuffer.length());
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
                    process(static_cast<MessageType>(buffer.data()[0]), buffer.data()+3, len);
                    buffer.remove(whole);
                }
                else
                    break;
            }
        }
    }
}
