#include "Tunnel.hpp"
#include "IniFile.hpp"
#include "PrivilegedOperations.hpp"

int main(int argc, char **argv)
{
    PrivilegedOperations po;
    po.start();
    CHECK(setresgid(1000, 1000, 1000));
    CHECK(setresuid(1000, 1000, 1000));
    Logger::global()->info("Hi, I am unprivileged and my uid is {}", getuid());
    IniFile f;
    if(argc>1)
        f.load(argv[1]);
    else
        f.load("sshtun.ini");
    Config cfg(f);
    Tunnel t(cfg, po);
    t.work();
	return 0;
}
