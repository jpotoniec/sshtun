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
#include <wait.h>

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
    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));
    struct cmsghdr *cmsg;
    char buf[CMSG_SPACE(sizeof(fd))];  /* ancillary data buffer */

    msg.msg_control = buf;
    msg.msg_controllen = sizeof buf;
    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(fd));
    /* Initialize the payload: */
    memcpy(CMSG_DATA(cmsg), &fd, sizeof(fd));
    /* Sum of the length of all control messages in the buffer: */
    msg.msg_controllen = cmsg->cmsg_len;
    CHECK(sendmsg(sock, &msg, 0));
    Logger::global()->debug("Send tunnel descriptor {}", fd);
}

static int recv_fd(int sock)
{
    struct msghdr msg;
    memset(&msg, 0, sizeof(msg));
    struct cmsghdr *cmsg;
    int fd;
    char buf[CMSG_SPACE(sizeof(fd))];  /* ancillary data buffer */

    msg.msg_control = buf;
    msg.msg_controllen = sizeof(buf);
    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(fd));
    /* Sum of the length of all control messages in the buffer: */
    msg.msg_controllen = cmsg->cmsg_len;
    CHECK(recvmsg(sock, &msg, 0));
    fd=*reinterpret_cast<int*>(CMSG_DATA(cmsg));
    Logger::global()->debug("Recv tunnel descriptor {}", fd);
    return fd;
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
                processCreateTunnel(p.tunnel.name, p.tunnel.local, p.tunnel.remote);
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

static void fill(sockaddr &dst, const std::string& ip)
{
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family=AF_INET;
    in_addr_t x=inet_addr(ip.c_str());
    memcpy(&addr.sin_addr, &x, sizeof(x));
    memcpy(&dst, &addr, sizeof(addr));
}

void PrivilegedOperations::processCreateTunnel(const char *name, const char *local, const char *remote)
{
    this->router=remote;
    Logger::global()->info("Setting up tunnel {}: {} -> {}", name, local, remote);
    int fd,helper;
    CHECK(fd=open("/dev/net/tun",O_RDWR|O_CLOEXEC));
    ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    ifr.ifr_flags = IFF_TUN;
    sensibleCopy(ifr.ifr_name,name,IFNAMSIZ);
    CHECK(ioctl(fd, TUNSETIFF, &ifr));
    CHECK(fcntl(fd, F_SETFL, O_NONBLOCK));
    CHECK(helper=socket(PF_INET, SOCK_DGRAM, IPPROTO_IP));
    fill(ifr.ifr_addr, local);
    CHECK(ioctl(helper, SIOCSIFADDR, &ifr));
    fill(ifr.ifr_addr, remote);
    CHECK(ioctl(helper, SIOCSIFDSTADDR, &ifr));
    CHECK(ioctl(helper, SIOCGIFFLAGS, &ifr));
    ifr.ifr_flags|=IFF_UP|IFF_RUNNING;
    CHECK(ioctl(helper, SIOCSIFFLAGS, &ifr));
    close(helper);
    send_fd(sock, fd);
    close(fd);
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
    WARN(waitpid(pid, NULL, 0));
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
