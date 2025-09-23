#include <iostream>
#include <string>
#include <format>
#include <chrono>
#include <mutex>
#include <unordered_map>
#include <memory>
#include <sqlite3.h>

#include "user_data.h"


const char *USERS_TABLE = R"(
    CREATE TABLE IF NOT EXISTS users (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        username TEXT NOT NULL UNIQUE,
        is_admin INTEGER NOT NULL DEFAULT 0,
        connected_at DATETIME DEFAULT CURRENT_TIMESTAMP
    );
)";

const char *MESSAGES_TABLE = R"(
    CREATE TABLE IF NOT EXISTS messages (
        id INTEGER PRIMARY KEY AUTOINCREMENT,
        user_id INTEGER NOT NULL,
        text TEXT NOT NULL,
        sent_at DATETIME DEFAULT CURRENT_TIMESTAMP,
        FOREIGN KEY(user_id) REFERENCES users(id) ON DELETE CASCADE
    );
)";

// Create "root" admin user (if does not exist)
const char *USER_ROOT = R"(
    INSERT OR IGNORE INTO users (username, is_admin)
    VALUES ('root', 1);
)";

 // Definition for the pure abstract destructor
UserDatabase::~UserDatabase()
{}

class UserDatabaseImpl : public UserDatabase {
    sqlite3* m_db = nullptr;

    // Map of user-name to UserData and guard mutex
    std::unordered_map<std::string, std::shared_ptr<UserData>> m_user_cache;
    // Note:
    // m_user_cache_mutex can be locked while clients_mutex is locked,
    // esp. in delete user scenario (dead-lock notice).
    std::mutex m_user_cache_mutex;

public:
    UserDatabaseImpl(sqlite3 *db) : m_db(db) {
    }
    virtual ~UserDatabaseImpl() {
        if (m_db) {
            sqlite3_close(m_db);
        }
    }

    virtual std::shared_ptr<UserData> find_user(const std::string &name, bool do_create);
    virtual bool delete_user(const UserData &user);

    bool store_chat(int user_id, const UserData::TimePoint &sent_at, const std::string &text);
    bool set_admin(int user_id, bool is_admin);
};

std::shared_ptr<UserDatabase> open_user_dadabase(const std::string &path) {
    // Check Threading Mode, must be:
    // Serialized - Fully thread-safe. Multiple threads can safely use the same connection.
    int mode = sqlite3_threadsafe();
    if (mode != 1) {
        std::cout << "Unsupported SQLite Threading Mode: " << mode << " (must be 1)" << std::endl;
    }

    sqlite3* db = nullptr;
    if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
        std::cerr << "Unable to open database: " << path << std::endl;
        return nullptr;
    }

    sqlite3_exec(db, USERS_TABLE, nullptr, nullptr, nullptr);
    sqlite3_exec(db, MESSAGES_TABLE, nullptr, nullptr, nullptr);
    sqlite3_exec(db, USER_ROOT, nullptr, nullptr, nullptr);

    std::cout << "Connected to user database: " << path << std::endl;
    return std::shared_ptr<UserDatabase>(new UserDatabaseImpl(db));
}

std::shared_ptr<UserData> UserDatabaseImpl::find_user(const std::string &name, bool do_create) {
    if (name.empty()) {
        return nullptr;
    }

    // Guard access to database
    std::lock_guard<std::mutex> lock(m_user_cache_mutex);

    /*
     * First check the cache
     */
    auto it = m_user_cache.find(name);
    if (it != m_user_cache.end()) {
        return it->second;
    }

    /*
     * Then check the database
     */
    if (do_create) {
        // Insert a new enry in "users" table
        sqlite3_stmt* stmt = nullptr;
        int ret = sqlite3_prepare_v2(m_db, "INSERT OR IGNORE INTO users (username) VALUES (?)", -1, &stmt, nullptr);
        // Bind username as text
        if (ret == SQLITE_OK) {
            ret = sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
        }
        // Execute the statement
        if (ret == SQLITE_OK) {
            ret = sqlite3_step(stmt);
        }
        sqlite3_finalize(stmt);

        if (ret != SQLITE_DONE) {
            std::cerr << "SQLite INSERT failed: " << sqlite3_errmsg(m_db) << std::endl;
            return nullptr;
        }
    }

    // Retrieve entry from "users" table
    sqlite3_stmt* stmt = nullptr;
    int ret = sqlite3_prepare_v2(m_db, "SELECT id, username, is_admin FROM users WHERE username = ?", -1, &stmt, nullptr);
    // Bind username as text
    if (ret == SQLITE_OK) {
        ret = sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    }
    // Execute the statement
    int db_id = 0;
    bool is_admin = false;
    std::string db_name;
    if (ret == SQLITE_OK) {
        ret = sqlite3_step(stmt);
        if (ret == SQLITE_ROW) {
            db_id = sqlite3_column_int(stmt, 0);
            db_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            is_admin = sqlite3_column_int(stmt, 2) != 0;
        }
    }
    sqlite3_finalize(stmt);

    if (ret != SQLITE_ROW) {
        std::cerr << "SQLite SELECT failed: " << sqlite3_errmsg(m_db) << std::endl;
        return nullptr;
    }

    // Update cache
    auto user = std::make_shared<UserData>(*this, db_id, db_name, is_admin);
    m_user_cache[name] = user;
    return user;
}

bool UserDatabaseImpl::delete_user(const UserData &user) {
    // Guard access to database
    std::lock_guard<std::mutex> lock(m_user_cache_mutex);
    auto it = m_user_cache.find(user.get_name());
    if (it != m_user_cache.end()) {
        m_user_cache.erase(it);
    }

    // TODO: Delete from database
    return true;
}

bool UserDatabaseImpl::store_chat(int user_id,
        const UserData::TimePoint &sent_at, const std::string &text) {
    // Insert a new enry in "messages" table
    sqlite3_stmt* stmt = nullptr;
    int ret = sqlite3_prepare_v2(m_db, "INSERT INTO messages (user_id, text, sent_at) VALUES (?, ?, ?)", -1, &stmt, nullptr);
    // Bind user_id as integer
    if (ret == SQLITE_OK) {
        ret = sqlite3_bind_int(stmt, 1, user_id);
    }
    // Bind chat-text as text
    if (ret == SQLITE_OK) {
        ret = sqlite3_bind_text(stmt, 2, text.c_str(), -1, SQLITE_TRANSIENT);
    }
    // Bind sent_at as text
    if (ret == SQLITE_OK) {
        // Round sent_at to to second precision
        std::string iso8601 = std::format("{:%F %T}",
                std::chrono::floor<std::chrono::seconds>(sent_at));
        ret = sqlite3_bind_text(stmt, 3, iso8601.c_str(), -1, SQLITE_TRANSIENT);
    }
    // Execute the statement
    if (ret == SQLITE_OK) {
        ret = sqlite3_step(stmt);
    }
    sqlite3_finalize(stmt);

    if (ret != SQLITE_DONE) {
        std::cerr << "SQLite INSERT failed: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }
    return true;
}

bool UserDatabaseImpl::set_admin(int user_id, bool is_admin) {
    // Update existing enry in "users" table
    sqlite3_stmt* stmt = nullptr;
    int ret = sqlite3_prepare_v2(m_db, "UPDATE users SET is_admin = ? WHERE id = ?", -1, &stmt, nullptr);
    // Bind is_admin boolean as integer
    if (ret == SQLITE_OK) {
        ret = sqlite3_bind_int(stmt, 1, is_admin ? 1 : 0);
    }
    // Bind user_id as integer
    if (ret == SQLITE_OK) {
        ret = sqlite3_bind_int(stmt, 2, user_id);
    }
    // Execute the statement
    if (ret == SQLITE_OK) {
        ret = sqlite3_step(stmt);
    }
    sqlite3_finalize(stmt);

    if (ret != SQLITE_DONE) {
        std::cerr << "SQLite UPDATE failed: " << sqlite3_errmsg(m_db) << std::endl;
        return false;
    }
    return true;
}


UserData::UserData(UserDatabaseImpl &database, int db_id, const std::string &name, bool is_admin) :
        m_database(database), m_db_id(db_id), m_name(name), m_is_admin(is_admin) {
}

UserDatabase &UserData::get_database() const {
    return m_database;
}

bool UserData::set_admin(bool is_admin) {
    if (!m_database.set_admin(m_db_id, is_admin)) {
        return false;
    }
    m_is_admin = is_admin;
    return true;
}

bool UserData::store_chat(const TimePoint &sent_at, const std::string &text) {
    return m_database.store_chat(m_db_id, sent_at, text);
}
