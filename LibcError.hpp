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

#define CHECK(call) if((call)<0) { throw LibcError(#call); }

#endif // LIBCERROR_HPP