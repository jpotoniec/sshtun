#ifndef PRIVILEGEDOPERATIONS_HPP
#define PRIVILEGEDOPERATIONS_HPP

#include <boost/noncopyable.hpp>
#include <string>

class PrivilegedOperations : private boost::noncopyable
{
public:
    void start();
    int createTunnel(const std::string& name, const std::string& local, const std::string& remote);
private:
    int sock;
    void work();
};

#endif // PRIVILEGEDOPERATIONS_HPP
