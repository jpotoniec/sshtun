#ifndef BUFFER_HPP
#define BUFFER_HPP

#include "LibcError.hpp"
#include "Logger.hpp"
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
            if(n==-1)
                throw LibcError("read");
            _pos+=n;
            return n;
        }
        ssize_t write(int fd)
        {
            const size_t max=63000;
            ssize_t n;
            CHECK(n=::write(fd, _data, std::min(_pos,max)));
            remove(n);
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
        char* data()
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
        void reset()
        {
            _pos=0;
        }
        void append(bool mustFit, char ch, uint16_t x, const void *data, size_t len)
        {
            if(_size-_pos<len+sizeof(ch)+sizeof(x))
            {
                if(mustFit)
                    throw std::runtime_error("Not enough space in the buffer"); //TODO: resize buffer
                else
                    return; //drop packet oO
            }
            memcpy(_data+_pos, &ch, sizeof(ch));
            _pos+=sizeof(ch);
            memcpy(_data+_pos, &x, sizeof(x));
            _pos+=sizeof(x);
            memcpy(_data+_pos, data, len);
            _pos+=len;
        }
        void append(const void *param, size_t n)
        {
            const char *data=static_cast<const char*>(param);
            if(_size-_pos<n)
                throw std::runtime_error("Not enough space in the buffer");
            memcpy(_data+_pos, data, n);
            _pos+=n;
        }
        bool isEmpty()
        {
            return _pos==0;
        }
    private:
        size_t _size;
        size_t _pos;
        char *_data;
};

#endif // BUFFER_HPP
