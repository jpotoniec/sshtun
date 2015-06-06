#include "Config.hpp"
#include "Logger.hpp"

static int toInt(const std::string& value, int def)
{
    try
    {
        return std::stoi(value);
    }
    catch(const std::exception&)
    {
        return def;
    }
}

template<typename T>
static void append(T& dst, const T& src)
{
    dst.insert(src.begin(), src.end());
}

void Config::load(const IniFile& ini)
{
    Lock lock(mutex);
    _name=ini("","name");
    if(_name.empty())
    {
        char buf[1024];
        CHECK(gethostname(buf, sizeof(buf)));
        _name=buf;
    }
    _proxyCommand=ini("","server");
    Logger::global()->trace("Using proxy command '{}'",_proxyCommand);
    append(_clients, ini["clients"]);
    _ip=ini("","ip",_ip);
    _router=toInt(ini("","router"),_router);
    append(_others, ini["others"]);
    _loglevel=ini("","loglevel",_loglevel);
    _unprivilegedUser=ini("","user",_unprivilegedUser);
    _breakLength=toInt(ini("","break",""), _breakLength);
    if(_ip.empty()==_proxyCommand.empty())
        throw ConfigError("Exactly one value must be defined: `ip' or `server'");
    if(_ip.empty() && _router)
        throw ConfigError("Only a main node should be a router");
}

void Config::addClient(const std::string& name, const std::string& ip)
{
    Logger::global()->trace("Discovered new client: {} {}", name, ip);
    Lock lock(mutex);
    _clients[name]=ip;
}
