#include "mySQLConnectionPool.h"
#include <mysql_driver.h>
#include <cppconn/connection.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <stdexcept>
#include <vector>
#include <memory>

MySQLConnectionPool::MySQLConnectionPool(const std::string& host, const std::string& user, const std::string& password, const std::string& database, int poolSize, int heartbeatInterval)
    : host_(host), user_(user), password_(password), database_(database), poolSize_(poolSize), heartbeatInterval_(heartbeatInterval), heartbeatRunning_(true) {
    sleep(60);
    driver_ = sql::mysql::get_mysql_driver_instance();
    initializePool();
    startHeartbeat();
}

MySQLConnectionPool::~MySQLConnectionPool() {
    stopHeartbeat();
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& conn : connectionPool_) {
        delete conn;
    }
}

sql::Connection* MySQLConnectionPool::getConnection() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!connectionPool_.empty()) {
        sql::Connection* conn = connectionPool_.back();
        connectionPool_.pop_back();
        if (conn && conn->isValid()) {
            return conn;
        } else {
            delete conn;
            // Create a new connection if the pooled one is invalid
            conn = driver_->connect(host_, user_, password_);
            conn->setSchema(database_);
            return conn;
        }
    }
    // Create a new connection if the pool is empty
    sql::Connection* conn = driver_->connect(host_, user_, password_);
    conn->setSchema(database_);
    return conn;
}

void MySQLConnectionPool::releaseConnection(sql::Connection* conn) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (conn && conn->isValid()) {
        connectionPool_.push_back(conn);
    } else {
        delete conn;
    }
}
void MySQLConnectionPool::initializePool() {
    for (int i = 0; i < poolSize_; ++i) {
        sql::Connection* conn = driver_->connect(host_, user_, password_);
        conn->setSchema(database_);
        connectionPool_.push_back(conn);
    }
}

void MySQLConnectionPool::startHeartbeat() {
    heartbeatThread_ = std::thread([this]() {
        while (heartbeatRunning_) {
            checkConnections();
            std::this_thread::sleep_for(std::chrono::seconds(heartbeatInterval_));
        }
    });
}

void MySQLConnectionPool::stopHeartbeat() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        heartbeatRunning_ = false;
    }
    if (heartbeatThread_.joinable()) {
        heartbeatThread_.join();
    }
}

void MySQLConnectionPool::checkConnections() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = connectionPool_.begin(); it != connectionPool_.end(); ) {
        if (!(*it)->isValid()) {
            delete *it;
            it = connectionPool_.erase(it);
        } else {
            ++it;
        }
    }
}
