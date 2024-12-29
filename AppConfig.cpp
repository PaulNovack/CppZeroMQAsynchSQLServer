#include "AppConfig.h"


namespace PaulNovack {

  AppConfig::AppConfig(const string& filePath) {
    ifstream file(filePath);
    if (!file.is_open()) {
      cerr << "Failed to open .env file." << endl;
      return;
    }
    string line;
    while (getline(file, line)) {
      size_t delimiterPos = line.find('=');
      if (delimiterPos != string::npos) {
        string key = line.substr(0, delimiterPos);
        string value = line.substr(delimiterPos + 1);
        if (key == "DB_DATABASE_NAME")
          DB_DATABASE_NAME = value;
        else if (key == "DB_USERNAME")
          DB_USERNAME = value;
        else if (key == "DB_PASSWORD")
          DB_PASSWORD = value;
        else if (key == "DB_HOST")
          DB_HOST = value;
        else if (key == "DB_HEARTBEAT_INTERVAL")
          DB_HEARTBEAT_INTERVAL = stoi(value);
        else if (key == "DB_POOL_SIZE")
          DB_POOL_SIZE = stoi(value);
      }
    }
    file.close();
  }
}