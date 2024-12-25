#include "DataStore.h"


namespace PaulNovack {

  DataStore::DataStore(const AppConfig& config) {
    connectionPool_ = new MySQLConnectionPool(config.DB_HOST,
            config.DB_USERNAME,
            config.DB_PASSWORD,
            config.DB_DATABASE_NAME,
            config.DB_POOL_SIZE,
            config.DB_HEARTBEAT_INTERVAL);
  }

  DataStore::DataStore(const DataStore& orig) {
  }

  DataStore::~DataStore() {
  }

}
