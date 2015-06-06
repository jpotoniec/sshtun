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
    typedef std::map<std::string, std::string> Map;
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
    Map others() const
    {
        Lock lock(mutex);
        return _others;
    }
    std::string proxyCommand() const
    {
        return _proxyCommand;
    }
    Map clients() const
    {
        Lock lock(mutex);
        return _clients;
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
    Map env() const
    {
        Lock lock(mutex);
        return _env;
    }
private:
    typedef std::mutex Mutex;
    typedef std::lock_guard<Mutex> Lock;
    mutable Mutex mutex;
    Config()
        :_breakLength(5),_router(false),_unprivilegedUser("sshtun"),_loglevel("info")
    {
        _env["SSH"]="ssh -o ServerAliveInterval=30 -o ServerAliveCountMax=5 -o TCPKeepAlive=yes -o EscapeChar=none -o PasswordAuthentication=no";
    }
    std::string _name;
    std::string _proxyCommand;
    std::string _ip;
    Map _clients,_others;
    int _breakLength;
    bool _router;
    std::string _unprivilegedUser;
    std::string _loglevel;
    Map _env;
};

#endif // CONFIG_HPP
