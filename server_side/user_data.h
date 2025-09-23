/*
 * UserData class declaration
 */


class UserData {
    std::string m_name;
    bool m_is_admin = true;

public:
    UserData();

    void construct(const std::string &name);
    std::string get_name() const { return m_name;}
    bool is_admin() const { return m_is_admin;}
    bool set_admin(bool is_admin) { m_is_admin = is_admin; return true;}

    using TimePoint = std::chrono::sys_time<std::chrono::nanoseconds>;
    bool store_chat(const TimePoint &sent_at, const std::string &text);
};

std::shared_ptr<UserData> find_user(const std::string &name, bool do_create);
