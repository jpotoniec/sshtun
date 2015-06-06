#ifndef TUNNEL_HPP
#define TUNNEL_HPP

#include "Buffer.hpp"
#include "Config.hpp"
#include "PrivilegedOperations.hpp"

struct ProxyConfig
{
public:
    std::string name;
    std::string command;
};

class Tunnel
{
    public:
        enum struct MessageType : char {PACKET, DHCP, HANDSHAKE, OTHER, ROUTE, OTHER_CLIENT};
        Tunnel(int client);
        ~Tunnel();
        void work();
        static void startTunnel(const ProxyConfig& proxy);
    private:
        pid_t pid;
        int localIn,localOut,tunnel;
        Buffer buffer,tunBuffer,sendBuffer;
        bool server;
        Tunnel(const ProxyConfig& proxy);
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
        void other(char *data, size_t len);
        void route(const char *data, size_t len);
        void otherClient(char *data, size_t len);
        void close();
};

#endif // TUNNEL_HPP
