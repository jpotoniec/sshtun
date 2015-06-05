#ifndef TUNNEL_HPP
#define TUNNEL_HPP

#include "Buffer.hpp"
#include "Config.hpp"
#include "PrivilegedOperations.hpp"

class Tunnel
{
    public:
        enum struct MessageType : char {PACKET, DHCP, HANDSHAKE, OTHER, ROUTE};
        Tunnel(int client);
        void work();
        static void startTunnel(const std::string& proxy);
    private:
        pid_t pid;
        int localIn,localOut,tunnel;
        Buffer buffer,tunBuffer;
        bool server;
        Tunnel(const std::string& proxy);
        Tunnel();
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
        void other(const char *data, size_t len);
        void route(const char *data, size_t len);
        void close();
};

#endif // TUNNEL_HPP
