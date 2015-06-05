#ifndef REGISTER_HPP
#define REGISTER_HPP

#include <mutex>
#include <boost/noncopyable.hpp>
#include <map>

class Register : private boost::noncopyable
{
public:
    static Register& get()
    {
        static Register me;
        return me;
    }
    void add(pid_t pid, int fd);
private:
    typedef std::lock_guard<std::mutex> Lock;

    std::mutex mutex;
    std::map<pid_t,int> data;

    Register();
    static void corpseHandler(int);
    void process(pid_t pid);
};

#endif // REGISTER_HPP
