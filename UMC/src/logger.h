#ifndef LOGGER_H
#define LOGGER_H

#include <queue>
#include <string>
#include <mutex>
#include <ArduinoJson.h>

class Logger
{
    public:
        // Singleton pattern to ensure only one instance of Logger
        static Logger& getInstance()
        {
            static Logger instance;
            return instance;
        }

        // Add a log message to the queue
        void log(const std::string& message)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            logQueue.push(message);
        }

        // Retrieve and remove the oldest log message from the queue
        bool getLog(std::string& message)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (logQueue.empty())
            {
                return false;
            }
            message = logQueue.front();
            logQueue.pop();
            return true;
        }

    private:
        // Private constructor to prevent instantiation
        Logger() {}

        // Disable copy constructor and assignment operator
        Logger(const Logger&) = delete;
        Logger& operator=(const Logger&) = delete;

        std::queue<std::string> logQueue;
        std::mutex mutex_;
};

#endif // LOGGER_H