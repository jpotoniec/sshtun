#ifndef TUNNEL_HPP
#define TUNNEL_HPP

#include "Buffer.hpp"
#include "Config.hpp"

class Tunnel
{
    public:
        Tunnel(const Config &cfg, int input, int output, bool server)
            :cfg(cfg), localIn(input),localOut(output),tunnel(-1),server(server),buffer(10000),tunBuffer(10000)
        { }
        void work();
    private:
        enum struct MessageType : char {PACKET, DHCP, HANDSHAKE};
        const Config &cfg;
        int localIn,localOut,tunnel;
        bool server;
        Buffer buffer,tunBuffer;
        void send(MessageType type, const char *data, uint16_t len);
        void send(MessageType type, const std::string& data)
        {
            send(type, data.c_str(), data.length()+1);
        }
        void process(MessageType type, char *data, size_t len);
        void deliver(const char *data, size_t len);
        void init(char *data, size_t len);
        void initServer(const char *data, size_t len);
        void handshake();
};

#endif // TUNNEL_HPP
