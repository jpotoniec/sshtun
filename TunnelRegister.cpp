#include "TunnelRegister.hpp"
#include "Tunnel.hpp"
#include <thread>

std::set<std::string> TunnelRegister::activeConnections;
TunnelRegister::Mutex TunnelRegister::mutex;

void TunnelRegister::startTunnelThread(const std::string& name, const std::string& proxy)
{
    if(!name.empty())
    {
        if(activeConnections.find(name)==activeConnections.end())
            activeConnections.insert(name);
        else
            return;
    }
    Lock lock(mutex);
    ProxyConfig cfg={name, proxy};
    std::thread(Tunnel::startTunnel, cfg).detach();
}
