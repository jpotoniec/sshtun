#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "IniFile.hpp"
#include "LibcError.hpp"
#include <unistd.h>
#include <boost/noncopyable.hpp>

class Config : boost::noncopyable
{
public:
    Config(const IniFile& f)
        :ini(f)
    {
        _name=ini("name");
        if(_name.empty())
        {
            char buf[1024];
            CHECK(gethostname(buf, sizeof(buf)));
            _name=buf;
        }
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
private:
    std::string _name;
    IniFile ini;
};

#endif // CONFIG_HPP
