#include <string>
#include <mutex>
#include <map>
#include "user_data.h"


// Map of user-name to UserData and guard mutex
//
// Note:
// user_database_mutex can be locked while clients_mutex is locked,
// esp. in delete user scenario (dead-lock notice).
std::map<std::string, UserData> g_user_database;
std::mutex user_database_mutex;

UserData *find_user(const std::string &name, bool do_create) {
    // Guard access to database
    std::lock_guard<std::mutex> lock(user_database_mutex);
    auto it = g_user_database.find(name);
    if (it != g_user_database.end()) {
        return &(it->second);
    }

    if (do_create) {
        // Create a new user element in database
        auto &result = g_user_database[name];
        result.construct(name);
        return &result;
    }

    return nullptr;
}

bool delete_user(const UserData &user) {
    // Guard access to database
    std::lock_guard<std::mutex> lock(user_database_mutex);
    auto it = g_user_database.find(user.get_name());
    if (it != g_user_database.end()) {
        return false;
    }

    g_user_database.erase(it);
    return true;
}

UserData::UserData() {
}

void UserData::construct(const std::string &name) {
    m_name = name;
}
