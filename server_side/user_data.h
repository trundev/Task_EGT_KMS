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
};

UserData *find_user(const std::string &name, bool do_create);
