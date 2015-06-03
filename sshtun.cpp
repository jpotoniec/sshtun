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

void copy(int src, int dst)
{
	char buf[70000];
	ssize_t sread=0, swrite=0;
	sread=read(src, buf, sizeof(buf));
	printf("%d->%d: %ld ", src, dst, sread);
	if(sread>0)
	{
		swrite=write(dst, buf, sread);
		if(swrite<0)
			printf("%s ",strerror(errno));
	}
	printf("%ld\n", swrite);
}

int main()
{
	char buffer[70000];
	int fd=open("/dev/net/tun",O_RDWR);
	printf("open=%d\n", fd);
	ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_flags = IFF_TUN; 
	strncpy(ifr.ifr_name,"smaug",IFNAMSIZ);
	int rv=ioctl(fd, TUNSETIFF, &ifr);
	printf("ioctl=%d errno=%d %s\n", rv, errno, strerror(errno));
	rv=fcntl(fd, F_SETFL, O_NONBLOCK);
	printf("fcntl=%d errno=%d %s\n", rv, errno, strerror(errno));
	rv=fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
	printf("fcntl=%d errno=%d %s\n", rv, errno, strerror(errno));
	for(;;)
	{
		pollfd fds[]={
			{fd,POLLIN,0},
			{STDIN_FILENO,POLLIN,0},
			{-1,0,0}
		};
		int n=poll(fds, sizeof(fds)/sizeof(fds[0]), -1);
		if(n<0)
		{
			printf("poll=%d errno=%d %s\n", n, errno, strerror(errno));
		}
		if(fds[0].revents&POLLIN)
			copy(fds[0].fd, fds[1].fd);
		if(fds[1].revents&POLLIN)
			copy(fds[1].fd, fds[0].fd);
	}
	close(fd);
	return 0;
}
