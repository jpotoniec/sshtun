#ifndef PRIVILEGEDOPERATIONS_HPP
#define PRIVILEGEDOPERATIONS_HPP

#include <boost/noncopyable.hpp>
#include <string>

class PrivilegedOperations : private boost::noncopyable
{
public:
    static PrivilegedOperations& get()
    {
        static PrivilegedOperations me;
        return me;
    }
    void start();
    int createTunnel(const std::string& name, const std::string& local, const std::string& remote);
    void addRoute(const std::string& route, const std::string& router);
private:
    PrivilegedOperations()
    {

    }
    void processAddRoute(const char *route, const char *router);
    void processCreateTunnel(const char *name, const char *local, const char *remote);
    int sock;
    void work();
};

#endif // PRIVILEGEDOPERATIONS_HPP
