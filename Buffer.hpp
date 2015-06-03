#ifndef BUFFER_HPP
#define BUFFER_HPP

#include "LibcError.hpp"
#include <boost/noncopyable.hpp>

class Buffer : private boost::noncopyable
{
    public:
        Buffer(size_t size)
            :_size(size),_pos(0),_data(new char[_size])
        {
        }
        ~Buffer()
        {
            delete []_data;
        }
        ssize_t read(int fd)
        {
            ssize_t n=::read(fd, _data+_pos, _size-_pos);
            fprintf(stderr, "read %ld\n", n);
            if(n==-1)
                throw LibcError("read");
#if 0
            for(size_t i=0;i<n;++i)
            {
                fprintf(stderr,"%02x ", static_cast<uint8_t>(_data[_pos+i]));
                if(i%16==15)
                    fprintf(stderr,"\n");
            }
            fprintf(stderr,"\n");
#endif
            _pos+=n;
            return n;
        }
        size_t size() const
        {
            return _size;
        }
        size_t length() const
        {
            return _pos;
        }
        const char* data() const
        {
            return _data;
        }
        void remove(size_t n)
        {
            if(_pos>n)
            {
                memmove(_data, _data+n, _size-n);
                _pos-=n;
            }
            else
                _pos=0;
        }
    private:
        size_t _size;
        size_t _pos;
        char *_data;
};

#endif // BUFFER_HPP
