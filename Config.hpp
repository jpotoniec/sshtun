#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "IniFile.hpp"
#include "Logger.hpp"
#include "LibcError.hpp"
#include <unistd.h>
#include <boost/noncopyable.hpp>
#include <mutex>

class Config : boost::noncopyable
{
public:
    static const std::string PROXY_ENV;
    static Config& get()
    {
        static Config me;
        return me;
    }
    void load(const IniFile& f);
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
    template<typename Fn>
    void clients(Fn f)
    {
        Lock lock(mutex);
        for(auto &i:_clients)
            f(i.first, i.second);
    }
    void addClient(const std::string& name, const std::string& ip)
    {
        Logger::global()->trace("Discovered new client: {} {}", name, ip);
        Lock lock(mutex);
        _clients[name]=ip;
    }
    int breakLength() const
    {
        return 5;   //seconds
    }
    bool isRouter() const
    {
        try
        {
            return std::stoi(ini("router"));
        }
        catch(const std::exception&)
        {
            return false;
        }
    }
private:
    typedef std::mutex Mutex;
    typedef std::lock_guard<Mutex> Lock;
    Mutex mutex;
    Config()
    {

    }
    std::string _name;
    std::string _proxyCommand;
    IniFile ini;
    std::map<std::string,std::string> _clients;
};

#endif // CONFIG_HPP
