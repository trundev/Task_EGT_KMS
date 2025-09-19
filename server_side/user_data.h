/*
 * UserData class declaration
 */


class UserData {
    std::string m_name;

public:
    UserData();

    void construct(const std::string &name);
    std::string get_name() const { return m_name;}
};

UserData *find_user(const std::string &name, bool do_create);
