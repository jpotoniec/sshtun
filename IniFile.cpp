#include "IniFile.hpp"
#include <fstream>
#include <iostream>

void IniFile::load(const std::string& file)
{
    std::string section="";
    std::ifstream f(file);
    while(f.good())
    {
        std::string line;
        std::getline(f, line);
        if(line.empty())
            continue;
        if(line[0]=='#')
            continue;
        if(line[0]=='[')
        {
            auto i=line.find("]");
            section=line.substr(1,i-1);
        }
        else
        {
            auto i=line.find("=");
            if(i!=std::string::npos)
            {
                std::string key=line.substr(0,i);
                std::string value=line.substr(i+1);
                std::cerr<<"'"<<section<<"' '"<<key<<"' '"<<value<<"'\n";
                values[section][key]=value;
            }
        }
    }
}
