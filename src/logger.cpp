#include "logger.h"
#include <fstream>
#include "settings.h"
#include <iostream>

static uint32_t CURRENT_LOG_LEVEL = LOG_LEVEL::LOG_INFO;
static std::ofstream logging_file;
static std::ofstream output_file;

void SetLogLevel(uint32_t log_level) {
    CURRENT_LOG_LEVEL = log_level;
}
void Log(const std::string& msg, LOG_LEVEL level) {
    if(!logging_file.is_open()) {
        logging_file.open(GetDllPath() + "log.txt", std::ios::out | std::ios::trunc);
    }
    if(logging_file.is_open()) {
        if(level <= CURRENT_LOG_LEVEL) {
            logging_file << msg << std::endl;
        }
    }
}

void WriteToOutputFile(const std::string& msg) {
    if(!output_file.is_open()) {
        output_file.open(GetDllPath() + "messages.json", std::ios::out | std::ios::trunc);
    }
    if(output_file.is_open()) {
        output_file << msg << std::endl;
    }
}

