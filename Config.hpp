#ifndef CONFIG_HPP
#define CONFIG_HPP

#include "IniFile.hpp"
#include "LibcError.hpp"
#include <unistd.h>
#include <boost/noncopyable.hpp>
#include <mutex>

class ConfigError : public std::logic_error
{
public:
    ConfigError(const std::string& what)
        :std::logic_error(what)
    { }
};

class Config : boost::noncopyable
{
public:
    static Config& get()
    {
        static Config me;
        return me;
    }
    void load(const IniFile& f);
    std::string name() const
    {
        Lock lock(mutex);
        return _name;
    }
    std::string ip() const
    {
        Lock lock(mutex);
        return _ip;
    }
    void setIp(const std::string& ip)
    {
        Lock lock(mutex);
        _ip=ip;
    }
    std::string loglevel() const
    {
        Lock lock(mutex);
        return _loglevel;
    }
    std::string logfile() const
    {
        throw std::runtime_error("logfile()");
//        return ini("logfile");
    }
    std::string ip(const std::string& client) const
    {
        Lock lock(mutex);
        auto i=_clients.find(client);
        if(i!=_clients.end())
            return i->second;
        else
            return "";
    }
    std::map<std::string,std::string> others() const
    {
        Lock lock(mutex);
        return _others;
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
    void addClient(const std::string& name, const std::string& ip);
    int breakLength() const
    {
        Lock lock(mutex);
        return _breakLength;
    }
    bool isRouter() const
    {
        Lock lock(mutex);
        return _router;
    }
    std::string unprivilegedUser() const
    {
        Lock lock(mutex);
        return _unprivilegedUser;
    }
private:
    typedef std::mutex Mutex;
    typedef std::lock_guard<Mutex> Lock;
    mutable Mutex mutex;
    Config()
        :_breakLength(5),_router(false),_unprivilegedUser("sshtun"),_loglevel("info")
    {

    }
    std::string _name;
    std::string _proxyCommand;
    std::string _ip;
    std::map<std::string,std::string> _clients,_others;
    int _breakLength;
    bool _router;
    std::string _unprivilegedUser;
    std::string _loglevel;
};

#endif // CONFIG_HPP
