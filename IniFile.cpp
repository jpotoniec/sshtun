#include "IniFile.hpp"
#include "Logger.hpp"
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
                Logger::global()->debug("Config: '{}' '{}' '{}'", section, key, value);
                values[section][key]=value;
            }
        }
    }
}

void IniFile::dump()
{
    for(auto &i:values)
    {
        std::cout<<"["<<i.first<<"]\n";
        for(auto &j:i.second)
            std::cout<<j.first<<"="<<j.second<<"\n";
    }
}
