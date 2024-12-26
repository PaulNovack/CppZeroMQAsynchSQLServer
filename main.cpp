#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <queue>
#include <map>
#include <future>
#include <mutex>
#include <condition_variable>
#include "cppzmq/zmq.hpp"
#include "mySQLConnectionPool.h"
#include "nlohmann/json.hpp"
#include <cppconn/statement.h>
#include <cppconn/resultset.h>
#include <cppconn/prepared_statement.h> // Required for PreparedStatement

using namespace std;
using json = nlohmann::json;

// Globals
mutex mtx;
condition_variable cv;
int responses = 0;
const int maxThreads = 75;

// Queue to hold pending requests
queue<tuple<string, string, string>> requestQueue;

// Initialize MySQL connection pool
const int poolSize = 75;
const int heartbeatInterval = 60;

MySQLConnectionPool connectionPool(
    "mysql:3306", // MySQL host
    "root",                 // MySQL user
    "password",             // MySQL password
    "testdb",               // MySQL database
    poolSize,               // Pool size
    heartbeatInterval       // Heartbeat interval
);

// Function to handle a single request
void handleRequest(zmq::socket_t &socket, const string &queryId, const string &query, const string &clientId) {
    sql::Connection* conn = nullptr;
    try {
        // Get a connection from the pool
        conn = connectionPool.getConnection();

        // Execute the query
        unique_ptr<sql::Statement> stmt(conn->createStatement());
        unique_ptr<sql::ResultSet> res(stmt->executeQuery(query));

        // Convert result set to JSON
        json results = json::array();
        while (res->next()) {
            json row = json::object();
            for (unsigned int i = 1; i <= res->getMetaData()->getColumnCount(); ++i) {
                string columnName = res->getMetaData()->getColumnLabel(i);
                string columnValue = res->getString(i);
                row[columnName] = columnValue;
            }
            results.push_back(row);
        }

        // Create the full JSON response
        json response = {
            {"id", queryId},
            {"data", results}
        };

        // Send the JSON response
        {
            lock_guard<mutex> lock(mtx);
            string responseString = response.dump(); // Serialize JSON to string
            socket.send(zmq::buffer(clientId), zmq::send_flags::sndmore);
            socket.send(zmq::buffer(responseString), zmq::send_flags::none);
            ++responses;
            if(responses % 500 == 0){
                cout << "Response Number: " << responses << " for Query ID: " << queryId << endl;            
            }
        }
    } catch (sql::SQLException &e) {
        cerr << "SQL Error for Query ID: " << queryId << ": " << e.what() << endl;
    } catch (const std::exception &e) {
        cerr << "General Error for Query ID: " << queryId << ": " << e.what() << endl;
    } catch (...) {
        cerr << "Unhandled exception during request processing." << endl;
    }
    if (conn) {
        connectionPool.releaseConnection(conn);
    }
}

// Worker thread function to process queued requests
void processQueue(zmq::socket_t &socket) {
    while (true) {
        tuple<string, string, string> request;
        {
            unique_lock<mutex> lock(mtx);
            cv.wait(lock, [] { return !requestQueue.empty(); });

            request = requestQueue.front();
            requestQueue.pop();
        }

        const string &queryId = get<0>(request);
        const string &query = get<1>(request);
        const string &clientId = get<2>(request);
        handleRequest(socket, queryId, query, clientId);
        std::this_thread::sleep_for(std::chrono::microseconds(20)); 
    }
}

void initializeDatabase() {
    sql::Connection* conn = nullptr;
    try {
        // Get a connection from the pool
        conn = connectionPool.getConnection();

        // Create the table if it doesn't exist
        unique_ptr<sql::Statement> stmt(conn->createStatement());
        stmt->execute("CREATE DATABASE IF NOT EXISTS testdb");
        stmt->execute("USE testdb");
        stmt->execute("DROP TABLE IF EXISTS users");
        stmt->execute(
            "CREATE TABLE users ("
            "id INT AUTO_INCREMENT PRIMARY KEY,"
            "name VARCHAR(100) NOT NULL,"
            "email VARCHAR(100) NOT NULL,"
            "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP)"
        );

        // Insert 5000 users into the table using a single SQL command
        std::string insertQuery = "INSERT INTO users (name, email) VALUES ";
        for (int i = 1; i <= 5000; ++i) {
            insertQuery += "('User " + std::to_string(i) + "', 'user" + std::to_string(i) + "@example.com')";
            if (i < 5000) {
                insertQuery += ", "; // Add a comma for all but the last entry
            }
        }
        
        // Execute the bulk insert
        stmt->execute(insertQuery);

        cout << "Database initialized and 5000 users added." << endl;
    } catch (sql::SQLException &e) {
        cerr << "SQL Error during database initialization: " << e.what() << endl;
    } catch (const std::exception &e) {
        cerr << "General error during database initialization: " << e.what() << endl;
    }
    if (conn) {
        connectionPool.releaseConnection(conn);
    }
}


int main() {
    initializeDatabase();
    zmq::context_t context(1);
    zmq::socket_t socket(context, ZMQ_ROUTER);
    socket.bind("tcp://0.0.0.0:5555");

    cout << "Server is running on tcp://0.0.0.0:5555" << endl;

    // Create worker threads
    vector<thread> workers;
    for (int i = 0; i < maxThreads; ++i) {
        workers.emplace_back([&socket]() {
            processQueue(socket);
        });
    }

    while (true) {
        zmq::message_t clientId;
        zmq::message_t emptyFrame;
        zmq::message_t message;

        // Receive client ID
        auto clientIdBytes = socket.recv(clientId, zmq::recv_flags::none);
        if (!clientIdBytes) {
            cerr << "Failed to receive client ID." << endl;
            continue;
        }

        // Receive empty frame
        auto emptyFrameBytes = socket.recv(emptyFrame, zmq::recv_flags::none);
        if (!emptyFrameBytes) {
            cerr << "Failed to receive empty frame." << endl;
            continue;
        }

        // Receive payload (JSON message)
        auto messageBytes = socket.recv(message, zmq::recv_flags::none);
        if (!messageBytes) {
            cerr << "Failed to receive message payload." << endl;
            continue;
        }

        string payload = string(static_cast<char *>(message.data()), message.size());
        if (payload.empty()) {
            cerr << "Received empty payload." << endl;
            continue;
        }

        try {
            // Parse JSON payload
            json received = json::parse(payload);
            if (received.contains("id") && received.contains("query")) {
                string queryId = received["id"];
                string query = received["query"];
                string clientIdStr(static_cast<char *>(clientId.data()), clientId.size());

                // Enqueue the request for processing
                {
                    lock_guard<mutex> lock(mtx);
                    requestQueue.emplace(queryId, query, clientIdStr);
                }
                cv.notify_one();
            } else {
                cerr << "Invalid message format received: " << payload << endl;
            }
        } catch (json::exception &e) {
            cerr << "JSON parsing error: " << e.what() << endl;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(20)); 
    }

    for (auto &worker : workers) {
        worker.join();
    }

    return 0;
}
