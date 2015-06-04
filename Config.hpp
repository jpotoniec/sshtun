#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "IniFile.hpp"
#include "LibcError.hpp"
#include <unistd.h>
#include <boost/noncopyable.hpp>

class Config : boost::noncopyable
{
public:
    Config(const IniFile& f, const std::string& proxyCommand="")
        :ini(f)
    {
        _name=ini("name");
        if(_name.empty())
        {
            char buf[1024];
            CHECK(gethostname(buf, sizeof(buf)));
            _name=buf;
        }
        if(proxyCommand.empty())
            _proxyCommand=ini("server");
        else
            _proxyCommand=proxyCommand;
    }
    std::string name() const
    {
        return _name;
    }
    std::string ip() const
    {
        return ini("ip");
    }
    std::string ip(const std::string& client) const
    {
        return ini("clients",client);
    }
    std::map<std::string,std::string> others() const
    {
        return ini["others"];
    }
    void setProxyCommand(const std::string& proxyCommand)
    {
        _proxyCommand=proxyCommand;
    }
    std::string proxyCommand() const
    {
        return _proxyCommand;
    }
    bool isServer() const
    {
        return proxyCommand().empty();
    }
private:
    std::string _name;
    std::string _proxyCommand;
    IniFile ini;
};

#endif // CONFIG_HPP
