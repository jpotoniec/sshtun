#ifndef INIFILE_HPP
#define INIFILE_HPP

#include <map>

class IniFile
{
public:
    typedef std::map<std::string,std::string> Section;
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
    Section operator[](const std::string& section) const
    {
        auto i=values.find(section);
        if(i!=values.end())
            return i->second;
        return Section();
    }
private:
    std::map<std::string,Section> values;
};

#endif // INIFILE_HPP
