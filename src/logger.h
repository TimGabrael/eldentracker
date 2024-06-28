#pragma once
#include <string>
#include <memory>

enum LOG_LEVEL {
    LOG_NONE,
    LOG_ERROR,
    LOG_WARNING,
    LOG_INFO,
};

template<typename ...Args>
std::string string_format(const std::string& format, Args... args) {
    int size_s = std::snprintf(nullptr, 0, format.c_str(), args...) + 1;
    if(size_s <= 0){ 
        return "";
    }
    auto size = static_cast<size_t>(size_s);
    std::unique_ptr<char[]> buf(new char[ size ]);
    std::snprintf(buf.get(), size, format.c_str(), args...);
    return std::string(buf.get(), buf.get() + size - 1);
}

void SetLogLevel(uint32_t log_level);
void Log(const std::string& msg, LOG_LEVEL level);

void WriteToOutputFile(const std::string& msg);




