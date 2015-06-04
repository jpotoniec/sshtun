#include "Tunnel.hpp"
#include "IniFile.hpp"

int main(int argc, char **argv)
{
    IniFile f;
    if(argc>1)
        f.load(argv[1]);
    else
        f.load("sshtun.ini");
    Config cfg(f);
    Tunnel t(cfg);
    t.work();
	return 0;
}
