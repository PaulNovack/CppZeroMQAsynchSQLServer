#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <iostream>
#include <fstream>

using namespace std;

namespace PaulNovack {

    class AppConfig {
    public:
        AppConfig(const std::string& filePath);
        string DB_DATABASE_NAME;
        string DB_USERNAME;
        string DB_PASSWORD;
        string DB_HOST;
        int DB_HEARTBEAT_INTERVAL;
        int DB_POOL_SIZE;
        
    private:
    };
}
#endif  // CONFIG_HPP
