#ifndef LOGGER_HPP
#define LOGGER_HPP

#include "spdlog/spdlog.h"

class Logger
{
public:
    static std::shared_ptr<spdlog::logger>& global()
    {
        static auto logger=spdlog::stderr_logger_mt("console");
        logger->set_level(spdlog::level::trace);
        return logger;
    }
private:
};

#endif // LOGGER_HPP
