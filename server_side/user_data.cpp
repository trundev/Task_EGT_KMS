#include <string>
#include <map>
#include "user_data.h"


std::map<std::string, UserData> g_user_database;

UserData *find_user(const std::string &name, bool do_create) {
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

UserData::UserData() {
}

void UserData::construct(const std::string &name) {
    m_name = name;
}
