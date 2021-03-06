#include "Tunnel.hpp"
#include "Logger.hpp"
#include "Register.hpp"
#include "TunnelRegister.hpp"
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

static pid_t popen2(const char* command, int &in, int &out, const std::string& name)
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
        WARN(close(cmdinput[0]));
        WARN(close(cmdinput[1]));
        WARN(close(cmdoutput[0]));
        WARN(close(cmdoutput[1]));
        WARN(setenv("NAME", name.c_str(), 1));
        for(auto &i:Config::get().env())
            WARN(setenv(i.first.c_str(), i.second.c_str(), 1));
        if(Logger::isDebugEnabled())
        {
            for(char **ptr=environ;*ptr;ptr++)
                Logger::global()->debug("env: {}", *ptr);
            char buf[PATH_MAX+1];
            if(getcwd(buf, sizeof(buf))!=NULL)
                Logger::global()->debug("cwd={}",buf);
        }
        CHECK(execl("/bin/sh", "sh", "-c", command, NULL));
        exit(0);
    }
    else
    {
        WARN(close(cmdoutput[1]));
        in=cmdoutput[0];
        WARN(close(cmdinput[0]));
        out=cmdinput[1];
    }
    return pid;
}

Tunnel::Tunnel(int client)
    :Tunnel()
{
    server=false;
    localIn=client;
    localOut=client;
}

Tunnel::Tunnel(const ProxyConfig& proxy)
    :Tunnel()
{
    server=true;
    pid=popen2(proxy.command.c_str(), localIn, localOut, proxy.name);
    Register::get().add(pid, localIn);
}

Tunnel::Tunnel()
    :pid(-1),localIn(-1),localOut(-1),tunnel(-1),buffer(1000000),tunBuffer(1000000),sendBuffer(1000000)
{
}

Tunnel::~Tunnel()
{
    close();
}

void Tunnel::close()
{
    if(pid>0)
    {
        kill(pid, SIGTERM);
        pid=-1;
    }
    if(tunnel>=0)
    {
        Logger::global()->trace("tunnel fd={}", tunnel);
        CHECK(::close(tunnel));
        tunnel=-1;
    }
    if(localIn>=0)
    {
        ::close(localIn);
        localIn=-1;
    }
    if(localOut>=0)
    {
        ::close(localOut);
        localOut=-1;
    }
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
    Config::get().setIp(ptrs[1]);
    this->remoteIp=ptrs[2];
    tunnel=PrivilegedOperations::get().createTunnel(ptrs[0], ptrs[1], ptrs[2]);
}

void Tunnel::initServer(const char *name, size_t len)
{
    if(name[len-1]!=0)
        return;
    std::string remoteIp=Config::get().ip(name);
    if(remoteIp.empty())
    {
        throw std::runtime_error(std::string("Unkown client '")+name+"'");
        return;
    }
    std::string msg=Config::get().name()+SEP+remoteIp+SEP+Config::get().ip();
    send(MessageType::DHCP, msg);
    tunnel=PrivilegedOperations::get().createTunnel(name, Config::get().ip(), remoteIp);
    for(auto& i:Config::get().others())
        if(name!=i.first)
            send(MessageType::OTHER, i.first+SEP+i.second);
    bool router=Config::get().isRouter();
    auto clients=Config::get().clients();
    for(auto& i:clients)
    {
        if(name!=i.first)
        {
            send(MessageType::OTHER_CLIENT, i.first+SEP+i.second);
            if(router)
                send(MessageType::ROUTE, i.second);
        }
    }
}

void Tunnel::other(char *name, size_t len)
{
    if(name[len-1]!='\0')
        return;
    char *proxyCommand=strchr(name, SEP);
    if(proxyCommand==NULL)
        return;
    *proxyCommand='\0';
    proxyCommand++;
    Logger::global()->trace("Tunnel::other({},{})", name, proxyCommand);
    TunnelRegister::startTunnelThread(name, proxyCommand);
}

void Tunnel::route(const char *data, size_t len)
{
    if(data[len-1]!=0)
        return;
    if(remoteIp.empty())
        return;
    PrivilegedOperations::get().addRoute(data, remoteIp);
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

void Tunnel::otherClient(char *name, size_t len)
{
    if(name[len-1]!='\0')
        return;
    char *ip=strchr(name, SEP);
    if(ip==NULL)
        return;
    *ip='\0';
    ip++;
    Config::get().addClient(name, ip);
}

void Tunnel::send(MessageType type, const char *data, uint16_t len)
{
    Logger::global()->trace("Sending {} bytes", len);
    char ch=static_cast<char>(type);
    uint16_t x=htons(len);
    sendBuffer.append(type!=MessageType::PACKET, ch, x, data, len);
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
    case Tunnel::MessageType::OTHER_CLIENT:
        return o<<"OTHER_CLIENT";
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
    case MessageType::OTHER_CLIENT:
        otherClient(data, len);
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
        send(MessageType::HANDSHAKE, Config::get().name());
    }
}

void Tunnel::work()
{
    handshake();
    for(;;)
    {
        short outFlag=(!sendBuffer.isEmpty()?POLLOUT:0)|POLLHUP|POLLERR;
        pollfd fds[]={
            {tunnel,POLLIN|POLLHUP|POLLERR,0},
            {localIn,POLLIN|POLLHUP|POLLERR|POLLRDHUP,0},
            {localOut,outFlag,0},
            {-1,0,0}
        };
        int nfds=sizeof(fds)/sizeof(fds[0]);
        int n=poll(fds, nfds, -1);
        if(n<0)
        {
            if(errno==EINTR)
                continue;
            else
                throw LibcError("poll");
        }
        for(int i=0;i<nfds;++i)
            if(fds[i].revents&(POLLHUP|POLLERR|POLLRDHUP|POLLNVAL))
            {
                Logger::global()->debug("I quit, fds[{}].revents={:x}", i, fds[i].revents);
                goto quit;
            }
        if(fds[0].revents&POLLIN)	// pakiet z lokalnego systemu, opakowac i wyekspediowac
        {
            tunBuffer.read(tunnel);
            send(MessageType::PACKET, tunBuffer.data(), tunBuffer.length());
            tunBuffer.reset();
        }
        if(fds[1].revents&POLLIN)	// zdalny pakiet, przetworzyc i wykonac albo dostarczyc
        {
            buffer.read(localIn);
            while(buffer.length()>=3)
            {
                size_t len=ntohs(*reinterpret_cast<const uint16_t*>(buffer.data()+1));
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
        if(fds[2].revents&POLLOUT)
        {
            sendBuffer.write(localOut);
        }
    }
    quit:
    Logger::global()->info("Client departed, closing tunnel");
    close();
}

void Tunnel::startTunnel(const ProxyConfig &proxy)
{
    for(;;)
    {
        Logger::global()->info("Starting tunnel to {} with {}", proxy.name, proxy.command);
        Tunnel t(proxy);
        try
        {
            t.work();
        }
        catch(const std::exception& ex)
        {
            Logger::global()->error("Tunnel died: {}", ex.what());
        }
        Logger::global()->info("Tunnel closed, waiting {} before reconnecting", Config::get().breakLength());
        sleep(Config::get().breakLength());
    }
}
