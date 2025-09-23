/*
 * UserData/UserDatabase class declarations
 */


class UserData;
class UserDatabaseImpl;

// Hide SQLite complexity, but allow object to be stack allocated
class UserDatabase {
public:
    virtual ~UserDatabase() = 0;

    virtual std::shared_ptr<UserData> find_user(const std::string &name, bool do_create) = 0;
    virtual bool delete_user(const UserData &user) = 0;
};
std::shared_ptr<UserDatabase> open_user_dadabase(const std::string &path);

class UserData {
    UserDatabaseImpl &m_database;
    int m_db_id;
    std::string m_name;
    bool m_is_admin;

public:
    UserData(UserDatabaseImpl &database, int db_id, const std::string &name, bool is_admin);

    std::string get_name() const { return m_name;}
    UserDatabase &get_database() const;
    bool is_admin() const { return m_is_admin;}
    bool set_admin(bool is_admin);

    using TimePoint = std::chrono::sys_time<std::chrono::nanoseconds>;
    bool store_chat(const TimePoint &sent_at, const std::string &text);
};
