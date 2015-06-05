#include "Tunnel.hpp"
#include "Logger.hpp"
#include "Register.hpp"
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
#include <sys/wait.h>
#include <cassert>

const char SEP='\x01';

static pid_t popen2(const char* command, int &in, int &out)
{
    int cmdinput[2],cmdoutput[2];
    CHECK(pipe(cmdinput));	//blocking
    CHECK(pipe(cmdoutput));
    pid_t pid;
    CHECK(pid=fork());
    if(pid==0)
    {
        CHECK(dup2(cmdoutput[1], STDOUT_FILENO));
        CHECK(dup2(cmdinput[0], STDIN_FILENO));
        execl("/bin/sh", "sh", "-c", command, NULL);
    }
    else
    {
        in=cmdoutput[0];
        out=cmdinput[1];
    }
    return pid;
}

//Tunnel *Tunnel::globalTunnelPtr=NULL;

//void Tunnel::corpseHandler(int)
//{
//    pid_t pid=waitpid(-1,NULL,WNOHANG);
//    if(globalTunnelPtr!=NULL)
//    {
//        if(pid==globalTunnelPtr->pid)
//        {
//            globalTunnelPtr->reconnect=true;
//        }
//    }
//}

Tunnel::Tunnel(Config &cfg, PrivilegedOperations& po, int client)
    :Tunnel(cfg, po)
{
    server=false;
    localIn=client;
    localOut=client;
}

Tunnel::Tunnel(Config &cfg, PrivilegedOperations& po, const std::string& proxy)
    :Tunnel(cfg, po)
{
    server=true;
    pid=popen2(proxy.c_str(), localIn, localOut);
    Register::get().add(pid, localIn);
}

Tunnel::Tunnel(Config &cfg, PrivilegedOperations& po)
    :cfg(cfg),po(po),pid(-1),localIn(-1),localOut(-1),tunnel(-1),buffer(10000),tunBuffer(10000)
{
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
    tunnel=po.createTunnel(ptrs[0], ptrs[1], ptrs[2]);
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
    tunnel=po.createTunnel(name, cfg.ip(), remoteIp);
    for(auto& i:cfg.others())
        if(name!=i.first)
            send(MessageType::OTHER, i.second);
    for(auto& i:cfg.clients())
        if(name!=i.first)
            send(MessageType::ROUTE, i.second);
}

void Tunnel::other(const char *proxyCommand, size_t len)
{
    if(proxyCommand[len-1]!='\0')
        return;
    pid_t pid;
    CHECK(pid=fork());
    if(pid==0)
    {
        CHECK(setenv(Config::PROXY_ENV.c_str(), proxyCommand, 1));
        CHECK(execv("/proc/self/exe", argv));
    }
    Logger::global()->debug("My child is {}", pid);
    sleep(10);
}

void Tunnel::route(const char *data, size_t len)
{
    if(data[len-1]!=0)
        return;
    po.addRoute(data);
}

void Tunnel::deliver(const char *data, size_t len)
{
    ssize_t n;
    while(len>0)
    {
        n=write(tunnel, data, len);
        Logger::global()->trace("Delivered {}/{} bytes", n, len);
        if(n<0)
            throw LibcError("write");
        data+=n;
        len-=n;
    }
}

void Tunnel::send(MessageType type, const char *data, uint16_t len)
{
    Logger::global()->trace("Sending {} bytes", len);
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
        Logger::global()->trace("Sent {}/{} bytes", n, len);
        data+=n;
        len-=n;
    }
    while(len>0);
}

std::ostream& operator<<(std::ostream& o, Tunnel::MessageType t)
{
    switch(t)
    {
    case Tunnel::MessageType::PACKET:
        return o<<"PACKET";
    case Tunnel::MessageType::DHCP:
        return o<<"DHCP";
    case Tunnel::MessageType::HANDSHAKE:
        return o<<"HANDSHAKE";
    case Tunnel::MessageType::OTHER:
        return o<<"OTHER";
    case Tunnel::MessageType::ROUTE:
        return o<<"ROUTE";
    default:
        return o<<static_cast<uint16_t>(t);
    }
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
    case MessageType::OTHER:
        other(data,len);
        break;
    case MessageType::ROUTE:
        route(data,len);
        break;
    default:
        Logger::global()->warn("Ignoring packet of type {} with length {}", type, len);
        break;
    }
}

void Tunnel::handshake()
{
    if(server)
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
            {localIn,POLLIN|POLLHUP|POLLERR|POLLRDHUP,0},
            {localOut,POLLHUP|POLLERR,0},
            {-1,0,0}
        };
        int nfds=sizeof(fds)/sizeof(fds[0]);
        int n=poll(fds, nfds, -1);
        Logger::global()->trace("poll={} [{:x} {:x} {:x}]", n, fds[0].revents, fds[1].revents, fds[2].revents);
        if(n<0)
        {
            if(errno==EINTR)
                continue;
            else
                throw LibcError("poll");
        }
        for(int i=0;i<nfds;++i)
            if(fds[i].revents&(POLLHUP|POLLERR|POLLRDHUP|POLLNVAL))
                goto quit;
        if(fds[0].revents&POLLIN)	// pakiet z lokalnego systemu, opakowac i wyekspediowac
        {
            if(tunBuffer.read(tunnel)==0)
                break;
            send(MessageType::PACKET, tunBuffer.data(), tunBuffer.length());
            tunBuffer.reset();
        }
        if(fds[1].revents&POLLIN)	// zdalny pakiet, przetworzyc i wykonac albo dostarczyc
        {
            if(buffer.read(localIn)==0)
                break;
            Logger::global()->trace("len={0}=0x{0:x}", buffer.length());
            while(buffer.length()>=3)
            {
                size_t len=ntohs(*reinterpret_cast<const uint16_t*>(buffer.data()+1));
                Logger::global()->trace("Packet length {0}=0x{0:x}, buffer length {1}=0x{1:x}", len, buffer.length());
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
    quit:
    Logger::global()->info("Client departed, closing tunnel");
    Logger::global()->trace("tunnel fd={}", tunnel);
    CHECK(::close(tunnel));
    ::close(localIn);
    ::close(localOut);
}
