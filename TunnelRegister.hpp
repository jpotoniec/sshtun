#ifndef TUNNELREGISTER_HPP
#define TUNNELREGISTER_HPP

#include <boost/noncopyable.hpp>
#include <mutex>
#include <set>

class TunnelRegister : private boost::noncopyable
{
public:
    static void startTunnelThread(const std::string& name, const std::string& proxy);
private:
    typedef std::mutex Mutex;
    typedef std::lock_guard<Mutex> Lock;
    static std::set<std::string> activeConnections;
    static Mutex mutex;
    TunnelRegister()
    {

    }
};

#endif // TUNNELREGISTER_HPP
