// I/O stub for the P053 refinement CLI. Not consensus. Lets RigorSupplyDelta
// reject-path LogPrintf link without the node logger.

#include <logging.h>

BCLog::Logger& LogInstance()
{
    static BCLog::Logger* g_logger{new BCLog::Logger()};
    return *g_logger;
}

void BCLog::Logger::LogPrintStr(const std::string&, const std::string&,
    const std::string&, int, BCLog::LogFlags, BCLog::Level)
{
}

bool BCLog::Logger::WillLogCategoryLevel(LogFlags, Level) const
{
    return false;
}
