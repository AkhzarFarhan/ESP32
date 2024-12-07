#ifndef LOGGER_H
#define LOGGER_H

#include <queue>
#include <string>
#include <mutex>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>

class Logger
{
public:
    struct LogEntry
    {
        std::string timestamp;
        float tempC;
        float tempF;
        float humidity;
        std::string log;
    };

    static Logger& getInstance()
    {
        static Logger instance;
        return instance;
    }

    void log(const LogEntry& log)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        LogEntry entry;
        entry.timestamp = getCurrentTime();
        entry.tempC = log.tempC;
        entry.tempF = log.tempF;
        entry.humidity = log.humidity;
        entry.log = log.log;
        logQueue_.push(entry);
    }

    bool get_log(LogEntry& entry)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (logQueue_.empty())
        {
            entry = {getCurrentTime(), 0, 0, 0, "No log"};
            return false;
        }
        entry = logQueue_.front();
        logQueue_.pop();
        return true;
    }

private:
    std::string getCurrentTime()
    {
        auto now = std::chrono::system_clock::now();
        auto in_time_t = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %X");
        return ss.str();
    }

    std::queue<LogEntry> logQueue_;
    std::mutex mutex_;
};

#endif // LOGGER_H