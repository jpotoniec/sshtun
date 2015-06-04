#include "PrivilegedOperations.hpp"
#include "Logger.hpp"
#include "LibcError.hpp"
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

struct CreateTunnelPacket
{
    char name[IFNAMSIZ];
    sockaddr_in local;
    sockaddr_in remote;
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
        CreateTunnelPacket p;
        ssize_t n=recv(sock, &p, sizeof(p), 0);
        assert(n==sizeof(p));
        Logger::global()->info("Setting up tunnel {}: {} -> {}", p.name, p.local, p.remote);
        int fd,helper;
        try
        {
            CHECK(fd=open("/dev/net/tun",O_RDWR));
            ifreq ifr;
            memset(&ifr, 0, sizeof(ifr));
            ifr.ifr_flags = IFF_TUN;
            strncpy(ifr.ifr_name,p.name,IFNAMSIZ);
            CHECK(ioctl(fd, TUNSETIFF, &ifr));
            CHECK(fcntl(fd, F_SETFL, O_NONBLOCK));
            CHECK(helper=socket(PF_INET, SOCK_DGRAM, IPPROTO_IP));
            memcpy(&ifr.ifr_addr, &p.local, sizeof(p.local));
            CHECK(ioctl(helper, SIOCSIFADDR, &ifr));
            memcpy(&ifr.ifr_addr, &p.remote, sizeof(p.remote));
            CHECK(ioctl(helper, SIOCSIFDSTADDR, &ifr));
            close(helper);
            send_fd(sock, fd);
            close(fd);
        }
        catch(const std::exception &e)
        {
            Logger::global()->error("{}", e.what());
        }
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
    CreateTunnelPacket p;
    memset(&p,0,sizeof(p));
    strncpy(p.name,name.c_str(),sizeof(p.name)-1);
    p.local={AF_INET, 0, inet_addr(local.c_str())};
    p.remote={AF_INET, 0, inet_addr(remote.c_str())};
    CHECK(send(sock, &p, sizeof(p), 0));
    return recv_fd(sock);
}
