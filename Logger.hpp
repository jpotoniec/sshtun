#ifndef LOGGER_HPP
#define LOGGER_HPP

#include "Config.hpp"
#include <spdlog/spdlog.h>
#include <boost/noncopyable.hpp>
#include <fstream>

class Logger : private boost::noncopyable
{
public:
    static std::shared_ptr<spdlog::logger>& global()
    {
        return me.logger;
    }
    static void configure()
    {
        std::string ll=cfg.loglevel();
        for(auto i=0;i<=static_cast<int>(spdlog::level::off);++i)
            if(ll==spdlog::level::level_names[i])
                global()->set_level(static_cast<spdlog::level::level_enum>(i));
        std::string file=cfg.logfile();
        if(!file.empty())
        {
            me.logger=spdlog::rotating_logger_mt("global", file, 1048576 * 5, 3);
        }
    }
private:
    static Logger me;
    std::shared_ptr<spdlog::logger> logger;
    Logger()
    {
        logger=spdlog::stderr_logger_mt("console");
        logger->set_pattern("[thread %t] %+");
    }
};

#endif // LOGGER_HPP
