#ifndef INIFILE_HPP
#define INIFILE_HPP

#include <map>

class IniFile
{
public:
    void load(const std::string& file);
    std::string operator()(const std::string& key) const
    {
        return operator()("", key);
    }
    std::string operator()(const std::string& section, const std::string& key) const
    {
        auto i=values.find(section);
        if(i!=values.end())
        {
            auto j=i->second.find(key);
            if(j!=i->second.end())
                return j->second;
        }
        return "";
    }
private:
    std::map<std::string,std::map<std::string,std::string>> values;
};

#endif // INIFILE_HPP
