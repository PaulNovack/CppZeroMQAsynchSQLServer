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
#include <cppconn/statement.h>
#include <cppconn/resultset.h>
#include <cppconn/prepared_statement.h> // Required for PreparedStatement
#include <msgpack.hpp> // MessagePack header

using namespace std;

// Globals
mutex mtx;
condition_variable cv;
int responses = 0;
const int maxThreads = 80;

// Queue to hold pending requests
queue<tuple<string, string, string>> requestQueue;

// Initialize MySQL connection pool
const int poolSize = 80;
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

        // Convert result set to MessagePack-compatible data
        vector<map<string, string>> results;
        while (res->next()) {
            map<string, string> row;
            for (unsigned int i = 1; i <= res->getMetaData()->getColumnCount(); ++i) {
                string columnName = res->getMetaData()->getColumnLabel(i);
                string columnValue = res->getString(i);
                row[columnName] = columnValue;
            }
            results.push_back(row);
        }

        // Serialize the response using MessagePack
        msgpack::sbuffer sbuf;
        msgpack::packer<msgpack::sbuffer> packer(sbuf);

        // Pack the response as a map
        packer.pack_map(2); // Two key-value pairs: "id" and "data"
        packer.pack("id");
        packer.pack(queryId); // Pack the query ID
        packer.pack("data");
        packer.pack(results); // Pack the results

        // Send the MessagePack response
        {
            lock_guard<mutex> lock(mtx);
            socket.send(zmq::buffer(clientId), zmq::send_flags::sndmore);
            socket.send(zmq::buffer(sbuf.data(), sbuf.size()), zmq::send_flags::none);
            ++responses;
            if (responses % 500 == 0) {
                cout << "Response Number: " << responses << " for Query ID: " << queryId << endl;
            }
        }
    } catch (sql::SQLException &e) {
        cerr << "SQL Error for Query ID: " << queryId << ": " << e.what() << endl;

        // Serialize the error response using MessagePack
        msgpack::sbuffer sbuf;
        msgpack::packer<msgpack::sbuffer> packer(sbuf);

        // Pack the error response as a map
        packer.pack_map(2); // Three key-value pairs: "id", "error", and "message"
        packer.pack("id");
        packer.pack(queryId);
        packer.pack("ERROR:SQLException");
        packer.pack(e.what());

        // Send the error response
        {
            lock_guard<mutex> lock(mtx);
            socket.send(zmq::buffer(clientId), zmq::send_flags::sndmore);
            socket.send(zmq::buffer(sbuf.data(), sbuf.size()), zmq::send_flags::none);
        }
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
        stmt->execute("DROP TABLE IF EXISTS person");
        stmt->execute(
            "CREATE TABLE person ("
            "id INT AUTO_INCREMENT PRIMARY KEY,"
            "name VARCHAR(100) NOT NULL,"
            "email VARCHAR(100) NOT NULL,"
            "created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP)"
        );

        // Insert 5000 users into the table using a single SQL command
        std::string insertQuery = "INSERT INTO person (name, email) VALUES ";
        for (int i = 1; i <= 100000; ++i) {
            insertQuery += "('User " + std::to_string(i) + "', 'user" + std::to_string(i) + "@example.com')";
            if (i < 100000) {
                insertQuery += ", "; // Add a comma for all but the last entry
            }
        }

        // Execute the bulk insert
        stmt->execute(insertQuery);
        
        insertQuery = "INSERT INTO person (name, email) VALUES ";
        for (int i = 100001; i <= 200000; ++i) {
            insertQuery += "('User " + std::to_string(i) + "', 'user" + std::to_string(i) + "@example.com')";
            if (i < 200000) {
                insertQuery += ", "; // Add a comma for all but the last entry
            }
        }

        // Execute the bulk insert
        stmt->execute(insertQuery);
        
        insertQuery = "INSERT INTO person (name, email) VALUES ";
        for (int i = 200001; i <= 300000; ++i) {
            insertQuery += "('User " + std::to_string(i) + "', 'user" + std::to_string(i) + "@example.com')";
            if (i < 300000) {
                insertQuery += ", "; // Add a comma for all but the last entry
            }
        }

        // Execute the bulk insert
        stmt->execute(insertQuery);
        
        insertQuery = "INSERT INTO person (name, email) VALUES ";
        for (int i = 300001; i <= 400000; ++i) {
            insertQuery += "('User " + std::to_string(i) + "', 'user" + std::to_string(i) + "@example.com')";
            if (i < 400000) {
                insertQuery += ", "; // Add a comma for all but the last entry
            }
        }

        // Execute the bulk insert
        stmt->execute(insertQuery);
        
        insertQuery = "INSERT INTO person (name, email) VALUES ";
        for (int i = 400001; i <= 500000; ++i) {
            insertQuery += "('User " + std::to_string(i) + "', 'user" + std::to_string(i) + "@example.com')";
            if (i < 500000) {
                insertQuery += ", "; // Add a comma for all but the last entry
            }
        }

        // Execute the bulk insert
        stmt->execute(insertQuery);

        cout << "Database initialized and 500000 persons added." << endl;
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

        // Receive payload (MessagePack message)
        auto messageBytes = socket.recv(message, zmq::recv_flags::none);
        if (!messageBytes) {
            cerr << "Failed to receive message payload." << endl;
            continue;
        }

        string payload(static_cast<char *>(message.data()), message.size());
        if (payload.empty()) {
            cerr << "Received empty payload." << endl;
            continue;
        }

        try {
            // Parse MessagePack payload
            msgpack::object_handle oh = msgpack::unpack(payload.data(), payload.size());
            msgpack::object received = oh.get();

            map<string, msgpack::object> receivedMap;
            received.convert(receivedMap);

            if (receivedMap.count("id") && receivedMap.count("query")) {
                string queryId = receivedMap["id"].as<string>();
                string query = receivedMap["query"].as<string>();
                string clientIdStr(static_cast<char *>(clientId.data()), clientId.size());

                // Enqueue the request for processing
                {
                    lock_guard<mutex> lock(mtx);
                    requestQueue.emplace(queryId, query, clientIdStr);
                }
                cv.notify_one();
            } else {
                cerr << "Invalid message format received." << endl;
            }
        } catch (const msgpack::unpack_error &e) {
            cerr << "MessagePack parsing error: " << e.what() << endl;
        }
        std::this_thread::sleep_for(std::chrono::microseconds(20));
    }

    for (auto &worker : workers) {
        worker.join();
    }

    return 0;
}
