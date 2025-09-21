/*
 * Logger class declaration
 */

class Logger {
    std::string m_curent_filename;
    std::ofstream m_logstream;

    Logger();

    static Logger &instance();
    std::chrono::system_clock::time_point select_logfile();

public:
    template <typename... Args>
    static void log(std::string_view fmt, Args&&... args) {
        instance().write(std::vformat(fmt, std::make_format_args(args...)));
    }

    void write(std::string_view log_message);
};
