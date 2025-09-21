/*
 * Logger class declaration
 */

class Logger {
    std::string m_curent_filename;
    std::ofstream m_logstream;

    Logger();

    static Logger &instance();
    using TimePoint = std::chrono::time_point<std::chrono::system_clock>;
    TimePoint select_logfile();

public:
    template <typename... Args>
    static void log(std::string_view fmt, Args&&... args) {
        instance().write(std::vformat(fmt, std::make_format_args(args...)));
    }

    void write(std::string_view log_message);
};
