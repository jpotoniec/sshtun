#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if_tun.h>
#include <linux/if.h>
#include <fcntl.h>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <cstdio>
#include <errno.h>
#include <poll.h>
#include <stdexcept>
#include <cstdint>
#include <arpa/inet.h>
#include <boost/noncopyable.hpp>

class LibcError : public std::runtime_error
{
	public:
		LibcError(const std::string& fn)
			:std::runtime_error(fn+": "+strerror(errno))
		{
		}
};

void process(char type, const char *data, size_t len)
{
	printf("Received packet %d of length %ld\n", type, len);
}

#define CHECK(call) if((call)<0) { throw LibcError(#call); }

int init_tunnel(const std::string& dev, const std::string& local, const std::string& remote)
{
	fprintf(stderr, "Setting up tunnel %s %s -> %s\n", dev.c_str(), local.c_str(), remote.c_str());
	int fd,sock;
	CHECK(fd=open("/dev/net/tun",O_RDWR));
	ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags = IFF_TUN; 
	strncpy(ifr.ifr_name,dev.c_str(),IFNAMSIZ);
	CHECK(ioctl(fd, TUNSETIFF, &ifr));
	CHECK(fcntl(fd, F_SETFL, O_NONBLOCK));
	CHECK(sock=socket(PF_INET, SOCK_DGRAM, IPPROTO_IP));
	sockaddr_in addr={AF_INET, 0, inet_addr(local.c_str())};
	memcpy(&ifr.ifr_addr, &addr, sizeof(addr));
	CHECK(ioctl(sock, SIOCSIFADDR, &ifr));
	addr={AF_INET, 0, inet_addr(remote.c_str())};
	memcpy(&ifr.ifr_addr, &addr, sizeof(addr));
	CHECK(ioctl(sock, SIOCSIFDSTADDR, &ifr));
	close(sock);
	return fd;
}

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

void Tunnel::init(const char *data, size_t len)
{
	const char *name=data;
	const char *src=name+strlen(name)+1;
	const char *dst=src+strlen(src)+1;
	tunnel=init_tunnel(name, src, dst);
}

void Tunnel::initServer(const char *data, size_t len)
{
	const char *name=data;
	tunnel=init_tunnel(name, "172.16.0.1", "172.16.0.2");
}

void Tunnel::deliver(const char *data, size_t len)
{
	ssize_t n;
	while(len>0)
	{
		n=write(tunnel, data, len);
		fprintf(stderr, "Delivered %ld/%ld\n", n, len);
		if(n<0)
			throw LibcError("write");
		data+=n;
		len-=n;
	}
}

void Tunnel::send(char type, const char *data, uint16_t len)
{
	fprintf(stderr,"Sending %d bytes\n", len);
	char buf[3];
	buf[0]=type;
	uint16_t x=htons(len);
	memcpy(buf+1, &x, sizeof(x));
	ssize_t n=write(localOut, buf, 3);
	if(n!=3)
		throw LibcError("write1");
	do
	{
		CHECK(n=write(localOut, data, len));
		fprintf(stderr,"Sent %d/%d bytes\n", n, len);
		data+=n;
		len-=n;
	}
	while(len>0);
}

void Tunnel::process(char type, const char *data, size_t len)
{
	switch(type)
	{
		case 0:
			deliver(data, len);
			break;
		case 1:
			init(data,len);
			break;
		case 2:
			initServer(data,len);
			break;
		default:
			fprintf(stderr,"Ignoring packet of type %d with length %ld\n", type, len);
			break;
	}
}

void Tunnel::handshake()
{
	if(server)
	{
		char data[]="srv\x00""172.16.0.2\x00""172.16.0.1";
		send(1, data, sizeof(data));
	}
	else
	{
		char data[]="client";
		send(2, data, sizeof(data));
	}
}

void Tunnel::work()
{
	handshake();
	for(;;)
	{
		pollfd fds[]={
			{tunnel,POLLIN,0},
			{localIn,POLLIN,0},
		};
		int n=poll(fds, sizeof(fds)/sizeof(fds[0]), -1);
		if(n<0)
		{
			if(errno==EINTR)
				continue;
			else
				throw LibcError("poll");
		}
		if(fds[0].revents&POLLIN)	// pakiet z lokalnego systemu, opakowac i wyekspediowac
		{
			tunBuffer.read(tunnel);
			send(0, tunBuffer.data(), tunBuffer.length());
			tunBuffer.remove(tunBuffer.length());
		}
		if(fds[1].revents&POLLIN)	// zdalny pakiet, przetworzyc i wykonac albo dostarczyc
		{
			buffer.read(localIn);
			fprintf(stderr,"len=%ld=0x%lx\n", buffer.length(), buffer.length());
			while(buffer.length()>=3)
			{
				size_t len=ntohs(*reinterpret_cast<const uint16_t*>(buffer.data()+1));
				fprintf(stderr, "Packet legnth %ld=0x%lx, buffer length %ld=0x%lx\n", len, len, buffer.length(), buffer.length());
				size_t whole=len+3;
				if(buffer.length()>=whole)
				{
					process(buffer.data()[0], buffer.data()+3, len); 
					buffer.remove(whole);
				}
				else
					break;
			}
		}
	}
}

void popen2(const char* command, int &in, int &out)
{
	int cmdinput[2],cmdoutput[2];
	CHECK(pipe(cmdinput));	//blocking
	CHECK(pipe(cmdoutput));
	pid_t pid;
	CHECK(pid=fork());
	if(pid==0)
	{
		CHECK(dup2(cmdoutput[1], STDOUT_FILENO));
		CHECK(dup2(cmdinput[0], STDIN_FILENO));
		execl("/bin/sh", "sh", "-c", command, NULL);
	}
	else
	{
		in=cmdoutput[0];
		out=cmdinput[1];
	}
}

int main(int argc, char **argv)
{
	int in=STDIN_FILENO;
	int out=STDOUT_FILENO;
	bool server=false;
	if(argc>1)
	{
		//w argv[1] jest proxy command
		popen2(argv[1], in, out);
		server=true;
	}
	CHECK(fcntl(in, F_SETFL, O_NONBLOCK));
	Tunnel t(in, out, server);
	t.work();
	return 0;
}
