#include "Config.hpp"
#include "Logger.hpp"

void Config::load(const IniFile& f)
{
    ini=f;
    _name=ini("name");
    if(_name.empty())
    {
        char buf[1024];
        CHECK(gethostname(buf, sizeof(buf)));
        _name=buf;
    }
    _proxyCommand=ini("server");
    Logger::global()->trace("Using proxy command '{}'",_proxyCommand);
    {
        Lock lock(mutex);
        auto other=ini["clients"];
        _clients.insert(other.begin(), other.end());
        _ip=ini("ip");
    }
}

void Config::addClient(const std::string& name, const std::string& ip)
{
    Logger::global()->trace("Discovered new client: {} {}", name, ip);
    Lock lock(mutex);
    _clients[name]=ip;
}
