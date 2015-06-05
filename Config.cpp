#include "Config.hpp"
#include "Logger.hpp"

const std::string Config::PROXY_ENV="SSHTUN_PROXY_COMMAND";

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
    const char *ptr=getenv(PROXY_ENV.c_str());    //TODO: secure_getenv?
    if(ptr!=NULL)
    {
        _proxyCommand=ptr;
        unsetenv(PROXY_ENV.c_str());
    }
    if(_proxyCommand.empty())
        _proxyCommand=ini("server");
    Logger::global()->trace("Using proxy command '{}'",_proxyCommand);
    {
        Lock lock(mutex);
        auto other=ini["clients"];
        _clients.insert(other.begin(), other.end());
    }
}
