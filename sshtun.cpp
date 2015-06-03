#include "Buffer.hpp"
#include "Tunnel.hpp"
#include "IniFile.hpp"
#include <unistd.h>
#include <fcntl.h>

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
#if 0
    IniFile f;
    f.load("sshtun.ini");
    printf("%s\n", f("clients","fruwacz").c_str());
#else
    bool server=(argc==1);
    IniFile f;
    if(server)
        f.load("sshtun.ini");
    Config cfg(f);
    fprintf(stderr,"Hi, my names is %s\n", cfg.name().c_str());
	int in=STDIN_FILENO;
    int out=STDOUT_FILENO;
    if(!server)
	{
		//w argv[1] jest proxy command i w takim razie jestesmy klientem laczacym sie do znanego serwera
		popen2(argv[1], in, out);
    }
	CHECK(fcntl(in, F_SETFL, O_NONBLOCK));
    Tunnel t(cfg, in, out, server);
	t.work();
#endif
	return 0;
}
