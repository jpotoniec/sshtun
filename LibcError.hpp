#ifndef LIBCERROR_HPP
#define LIBCERROR_HPP

#include <stdexcept>
#include <cstring>

class LibcError : public std::runtime_error
{
    public:
        LibcError(const std::string& fn)
            :std::runtime_error(fn+": "+strerror(errno))
        {
        }
};

#define CHECK(call) do { errno=0; if((call)<0) { throw LibcError(#call); } } while(0)
#define WARN(call) do { errno=0; if((call)<0) { Logger::global()->warn("{} failed: {} ({})", #call, errno, strerror(errno)); } } while(0)

#endif // LIBCERROR_HPP
