#include "Register.hpp"
#include <csignal>
#include <wait.h>

Register::Register()
{
    signal(SIGCHLD, Register::corpseHandler);
}

void Register::add(pid_t pid, int fd)
{
    Lock lock(mutex);
    data[pid]=fd;
}

void Register::process(pid_t pid)
{
    Lock lock(mutex);
    auto i=data.find(pid);
    if(i!=data.end())
    {
        close(i->second);   //may fail if i->second is already closed
        data.erase(i);
    }
}

void Register::corpseHandler(int)
{
    pid_t pid=waitpid(-1, NULL, WNOHANG);
    if(pid>0)
    {
        Register::get().process(pid);
    }
}
