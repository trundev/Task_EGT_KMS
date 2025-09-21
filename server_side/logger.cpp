/*
 * Logger class implementation
 */
#include <iostream>
#include <fstream>
#include <iomanip>
#include <string>
#include <format>
#include <chrono>
#include <mutex>

#include "logger.h"
#include "../common/defines.h"


#define LOG_FILENAME_FMT    "log_{:%Y-%m-%d %H_%M}.txt"


std::mutex log_mutex;

Logger::Logger() {
}

Logger& Logger::instance() {
    // Function static singleton for lazy initialization
    static Logger logger;
    return logger;
}

std::chrono::system_clock::time_point Logger::select_logfile() {
    // Must guard m_curent_filename comparison/assignment
    std::lock_guard<std::mutex> lock(log_mutex);
    auto now = std::chrono::system_clock::now();

    auto filename = std::format(LOG_FILENAME_FMT, time_point_cast<LOGFILE_TIME_ROUND>(now));
    if (m_curent_filename != filename) {
        m_curent_filename = filename;

        if (m_logstream.is_open()) {
            m_logstream << "** Change log-file to " << filename << std::endl;
            m_logstream.close();
        }
        std::cout << "New log-file: \"" << filename << "\"" << std::endl;
        m_logstream.open(m_curent_filename, std::ios::out | std::ios::app);
    }
    return now;
}

void Logger::write(std::string_view log_message) {
    auto now = select_logfile();

    std::ostringstream oss;
    oss << std::format("{:%Y-%m-%d %H:%M:%S} ", now);
    oss << log_message << std::endl;

    // Lock to avoid interleaved or corrupted output
    std::lock_guard<std::mutex> lock(log_mutex);
    m_logstream << oss.str() << std::flush;
}
