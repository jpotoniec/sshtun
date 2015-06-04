#include "PrivilegedOperations.hpp"
#include "Logger.hpp"
#include "LibcError.hpp"
#include "Utils.hpp"
#include <sys/socket.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if_tun.h>
#include <fcntl.h>
#include <arpa/inet.h>

struct Packet
{
    enum struct Type {CREATE_TUNNEL, ADD_ROUTE};
    Type type;
    union
    {
        struct
        {
            char name[IFNAMSIZ];
            char local[32];
            char remote[32];
        } tunnel;
        char route[64];
    };
};

static void send_fd(int sock, int fd)
{
    struct msghdr msg = {0};
    struct cmsghdr *cmsg;
    int myfds[]={fd}; /* Contains the file descriptors to pass. */
    int NUM_FD=sizeof(myfds)/sizeof(myfds[0]);
    char buf[CMSG_SPACE(sizeof myfds)];  /* ancillary data buffer */
    int *fdptr;

    msg.msg_control = buf;
    msg.msg_controllen = sizeof buf;
    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int) * NUM_FD);
    /* Initialize the payload: */
    fdptr = (int *) CMSG_DATA(cmsg);
    memcpy(fdptr, myfds, NUM_FD * sizeof(int));
    /* Sum of the length of all control messages in the buffer: */
    msg.msg_controllen = cmsg->cmsg_len;
    ssize_t n=sendmsg(sock, &msg, 0);
    Logger::global()->debug("Send tunnel descriptor {} in packet of {} bytes", fd, n);
}

static int recv_fd(int sock)
{
    struct msghdr msg = {0};
    struct cmsghdr *cmsg;
    int myfds[1]; /* Contains the file descriptors to pass. */
    int NUM_FD=sizeof(myfds)/sizeof(myfds[0]);
    char buf[CMSG_SPACE(sizeof myfds)];  /* ancillary data buffer */
    int *fdptr;

    msg.msg_control = buf;
    msg.msg_controllen = sizeof buf;
    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int) * NUM_FD);
    /* Sum of the length of all control messages in the buffer: */
    msg.msg_controllen = cmsg->cmsg_len;
    ssize_t n=recvmsg(sock, &msg, 0);
    fdptr = (int *) CMSG_DATA(cmsg);
    int fd=fdptr[0];
    Logger::global()->debug("Recv tunnel descriptor {} in packet of {} bytes", fd, n);
    return fd;
}

std::ostream& operator<<(std::ostream& o, const sockaddr_in &addr)
{
    char host[128],port[128];
    int n=getnameinfo(reinterpret_cast<const sockaddr*>(&addr), sizeof(addr), host, sizeof(host), port, sizeof(port), NI_NUMERICHOST|NI_NUMERICSERV);
    if(n==0)
        return o<<host<<":"<<port;
    else
        return o<<"?:?";
}

void PrivilegedOperations::work()
{
    for(;;)
    {
        Packet p;
        ssize_t n=recv(sock, &p, sizeof(p), 0);
        Logger::global()->debug("Privileged worker received {} bytes", n);
        if(n==0)
            break;
        assert(n==sizeof(p));
        try
        {
            if(p.type==Packet::Type::CREATE_TUNNEL)
            {
                this->router=p.tunnel.remote;
                Logger::global()->info("Setting up tunnel {}: {} -> {}", p.tunnel.name, p.tunnel.local, p.tunnel.remote);
                int fd,helper;
                CHECK(fd=open("/dev/net/tun",O_RDWR));
                ifreq ifr;
                memset(&ifr, 0, sizeof(ifr));
                ifr.ifr_flags = IFF_TUN;
                sensibleCopy(ifr.ifr_name,p.tunnel.name,IFNAMSIZ);
                CHECK(ioctl(fd, TUNSETIFF, &ifr));
                CHECK(fcntl(fd, F_SETFL, O_NONBLOCK));
                CHECK(helper=socket(PF_INET, SOCK_DGRAM, IPPROTO_IP));
                sockaddr_in addr;
                addr={AF_INET, 0, inet_addr(p.tunnel.local)};
                memcpy(&ifr.ifr_addr, &addr, sizeof(addr));
                CHECK(ioctl(helper, SIOCSIFADDR, &ifr));
                addr={AF_INET, 0, inet_addr(p.tunnel.remote)};
                memcpy(&ifr.ifr_addr, &addr, sizeof(addr));
                CHECK(ioctl(helper, SIOCSIFDSTADDR, &ifr));
                close(helper);
                sleep(2);   //hack to allow the interface to go up
                send_fd(sock, fd);
                close(fd);
            }
            if(p.type==Packet::Type::ADD_ROUTE)
            {
                processAddRoute(p.route);
            }
        }
        catch(const std::exception &e)
        {
            Logger::global()->error("{}", e.what());
        }
    }
    exit(0);
}

void PrivilegedOperations::processAddRoute(const char *route)
{
    Logger::global()->info("Adding route {} via {}", route, router);
    pid_t pid;
    CHECK(pid=fork());
    if(pid==0)
    {
        CHECK(execlp("ip","ip","r","a",route,"via",router.c_str(),"metric","100", NULL));
        exit(0);
    }
}

void PrivilegedOperations::start()
{
    int sockets[2];
    socketpair(PF_LOCAL, SOCK_SEQPACKET, 0, sockets);
    pid_t pid;
    CHECK(pid=fork());
    if(pid==0)
    {
        this->sock=sockets[0];
        close(sockets[1]);
        work();
        exit(0);
    }
    else
    {
        close(sockets[0]);
        this->sock=sockets[1];
    }
}

int PrivilegedOperations::createTunnel(const std::string& name, const std::string& local, const std::string& remote)
{
    Packet p;
    memset(&p,0,sizeof(p));
    p.type=Packet::Type::CREATE_TUNNEL;
    sensibleCopy(p.tunnel.name,name.c_str(),sizeof(p.tunnel.name));
    sensibleCopy(p.tunnel.local, local.c_str(), sizeof(p.tunnel.local));
    sensibleCopy(p.tunnel.remote, remote.c_str(), sizeof(p.tunnel.remote));
    CHECK(send(sock, &p, sizeof(p), 0));
    return recv_fd(sock);
}

void PrivilegedOperations::addRoute(const std::string &route)
{
    Packet p;
    memset(&p,0,sizeof(p));
    p.type=Packet::Type::ADD_ROUTE;
    sensibleCopy(p.route,route.c_str(),sizeof(p.route));
    CHECK(send(sock, &p, sizeof(p), 0));
}
