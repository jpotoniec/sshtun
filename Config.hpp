#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "IniFile.hpp"
#include "LibcError.hpp"
#include <unistd.h>
#include <boost/noncopyable.hpp>

class Config : boost::noncopyable
{
public:
    static const std::string PROXY_ENV;
    Config(const IniFile& f);
    std::string name() const
    {
        return _name;
    }
    std::string ip() const
    {
        return ini("ip");
    }
    std::string loglevel() const
    {
        return ini("loglevel");
    }
    std::string logfile() const
    {
        return ini("logfile");
    }
    std::string ip(const std::string& client) const
    {
        return ini("clients",client);
    }
    std::map<std::string,std::string> others() const
    {
        return ini["others"];
    }
    std::string proxyCommand() const
    {
        return _proxyCommand;
    }
    IniFile::Section clients() const
    {
        return ini["clients"];
    }
    int breakLength() const
    {
        return 5;   //seconds
    }
private:
    std::string _name;
    std::string _proxyCommand;
    IniFile ini;
};

#endif // CONFIG_HPP
