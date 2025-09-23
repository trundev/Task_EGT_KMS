#include <iostream>
#include <string>
#include <format>
#include <chrono>
#include <mutex>
#include <memory>
#include <map>
#include "user_data.h"


// Map of user-name to UserData and guard mutex
//
// Note:
// user_database_mutex can be locked while clients_mutex is locked,
// esp. in delete user scenario (dead-lock notice).
std::map<std::string, std::shared_ptr<UserData>> g_user_database;
std::mutex user_database_mutex;

std::shared_ptr<UserData> find_user(const std::string &name, bool do_create) {
    if (name.empty()) {
        return nullptr;
    }

    // Guard access to database
    std::lock_guard<std::mutex> lock(user_database_mutex);
    auto it = g_user_database.find(name);
    if (it != g_user_database.end()) {
        return it->second;
    }

    if (do_create) {
        // Create a new user element in database
        auto user = std::make_shared<UserData>();
        user->construct(name);
        g_user_database[name] = user;
        return user;
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

bool UserData::store_chat(const TimePoint &sent_at, const std::string &text) {
#if 0   // TODO: Implement chat storage, now just debug print
    std::cout << "TODO: Store chat from: " << m_name
            << std::format(", sent at UTC {:%Y-%m-%d %H:%M:%S}", sent_at)
            << ", text: " << text << std::endl;
#endif
    return false;
}
