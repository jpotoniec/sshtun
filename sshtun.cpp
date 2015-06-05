#include "Tunnel.hpp"
#include "IniFile.hpp"
#include "PrivilegedOperations.hpp"
#include "Utils.hpp"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>

const char SOCKET_PATH[]="/tmp/sshtun.sock";

Logger Logger::me;


/*
 * Ze sobą łączą się zawsze dwa *działające* serwery.
 * Jeden z nich inicjuje połączenie i wtedy ma dane w rurkach od ssh (a ssh na zdalnym serwerze wywoluje proxy, ktore przekazuje dane miedzy ssh a unix socketem),
 * drugi połączenie otrzymuje i wtedy ma dane w unix sockecie
 */

int mainClient()
{
    int sock;
    CHECK(sock=socket(AF_UNIX, SOCK_SEQPACKET|SOCK_CLOEXEC, 0));
    sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family=AF_UNIX;
    sensibleCopy(addr.sun_path,SOCKET_PATH,sizeof(addr.sun_path));
    CHECK(connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)));
    CHECK(fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK));
    CHECK(fcntl(sock, F_SETFL, O_NONBLOCK));
    Buffer buf01(7000000),buf10(7000000);
    for(;;)
    {
        short stdoutFlags=(!buf01.isEmpty()?POLLOUT:0)|POLLHUP|POLLERR;
        short sockFlags=(!buf10.isEmpty()?POLLOUT:0)|POLLIN|POLLHUP|POLLERR;
        pollfd fds[]=
        {
            {sock, sockFlags, 0},
            {STDIN_FILENO, POLLIN, 0},
            {STDOUT_FILENO, stdoutFlags, 0},
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
            if(fds[i].revents&(POLLHUP|POLLERR))
                return 0;
        if(fds[0].revents&POLLIN)
        {
            ssize_t n=buf01.read(sock);
            if(n==0)
                return 0;
        }
        if(fds[2].revents&POLLOUT)
            buf01.write(STDOUT_FILENO);
        if(fds[1].revents&POLLIN)
        {
            ssize_t n=buf10.read(STDIN_FILENO);
            if(n==0)
                return 0;
        }
        if(fds[0].revents&POLLOUT)
            buf10.write(sock);
    }
}

void processClient(int sock)
{
    Logger::global()->info("New client happened");
    Logger::global()->trace("socket is {}", sock);
    Tunnel t(sock);
    try
    {
        t.work();
    }
    catch(const std::exception& ex)
    {
        Logger::global()->error("Remote tunnel died: {}", ex.what());
    }
    close(sock);
}


#include <pwd.h>
void dropPrivileges()
{
    passwd *p=getpwnam(Config::get().unprivilegedUser().c_str());
    if(p==NULL)
        throw LibcError("getpwnam");
    uid_t uid=p->pw_uid;
    gid_t gid=p->pw_gid;
    CHECK(setresgid(uid, uid, uid));
    CHECK(setresuid(gid, gid, gid));
}

int mainServer(int argc, char **argv)
{
    Logger::global()->info("Started server");
    PrivilegedOperations::get().start();
    dropPrivileges();
    Logger::global()->debug("Hi, I am unprivileged and my uid is {}", getuid());
    IniFile f;
    if(argc>2)
        f.load(argv[2]);
    else
        f.load("sshtun.ini");
    Config::get().load(f);
    Logger::configure();
    if(!Config::get().proxyCommand().empty())
        std::thread(Tunnel::startTunnel, Config::get().proxyCommand()).detach();
    int sock;
    CHECK(sock=socket(AF_UNIX, SOCK_SEQPACKET|SOCK_CLOEXEC, 0));
    sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family=AF_UNIX;
    sensibleCopy(addr.sun_path,SOCKET_PATH,sizeof(addr.sun_path));
    unlink(addr.sun_path);
    CHECK(bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)));
    CHECK(listen(sock, 10));
    for(;;)
    {
        int client=accept4(sock, NULL, NULL, SOCK_NONBLOCK|SOCK_CLOEXEC);
        std::thread t(processClient, client);
        t.detach();
    }
}


int main(int argc, char **argv)
{
    if(argc>1 && strcmp(argv[1],"--server")==0)
    {
        return mainServer(argc, argv);
    }
    else
    {
        return mainClient();
    }
}
