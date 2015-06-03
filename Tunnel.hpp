#ifndef TUNNEL_HPP
#define TUNNEL_HPP

#include "Buffer.hpp"

class Tunnel
{
    public:
        Tunnel(int input, int output, bool server)
            :localIn(input),localOut(output),tunnel(-1),server(server),buffer(10000),tunBuffer(10000)
        { }
        void work();
    private:
        int localIn,localOut,tunnel;
        bool server;
        Buffer buffer,tunBuffer;
        void send(char type, const char *data, uint16_t len);
        void process(char type, const char *data, size_t len);
        void deliver(const char *data, size_t len);
        void init(const char *data, size_t len);
        void initServer(const char *data, size_t len);
        void handshake();
};

#endif // TUNNEL_HPP
