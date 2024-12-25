#ifndef DATASTORE_H
#define DATASTORE_H

#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/resultset.h>
#include <string>
#include <cmath>
#include <map>
#include <fstream>
#include <string>
#include <iostream>
#include "AppConfig.h"
#include <mutex>
#include "mySQLConnectionPool.h"
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>

using namespace std;

namespace PaulNovack {

    class DataStore {
    public:
        DataStore(const AppConfig& config);

        DataStore(const DataStore& orig);
        virtual ~DataStore();
    private:
        MySQLConnectionPool* connectionPool_;
    };
}
#endif /* DATASTORE_H */

