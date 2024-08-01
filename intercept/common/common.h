#pragma once 
#include <stdlib.h>
#include <stdint.h>
#include <chrono>
#include <iostream>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>

#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>

#include "spdlog/spdlog.h"
#include "spdlog/fmt/fmt.h"

namespace intercept {
namespace common {

#ifndef CLIENT_BUILD
const std::string CONFIG_FILE = "conf/server.conf";
#else
const std::string CONFIG_FILE = "conf/client.conf";
#endif

using Ino = uint64_t;
struct DirStream {
    Ino ino;
    uint64_t fh;
    uint64_t offset;
};

class Timer {
public:
    // Constructor starts the timer
    Timer();
    Timer(const std::string& message);

    // Destructor prints the elapsed time if the timer hasn't been stopped manually
    ~Timer();

    // Method to stop the timer and return the elapsed time in milliseconds
    void Stop();

    // Method to get the elapsed time in microseconds
    long long ElapsedMicroseconds() const;

    // Method to restart the timer
    void Restart();

private:
    std::chrono::time_point<std::chrono::high_resolution_clock> m_startTimePoint;
    long long m_elapsedTime = 0;
    bool m_stopped = false;
    std::string m_message;
};


class Configure {
public:
    // 获取单例实例的静态方法
    static Configure& getInstance() {
        static Configure instance;
        return instance;
    }

    // 加载配置文件的方法
    bool loadConfig(const std::string& filePath) {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            std::cerr << "Failed to open config file: " << filePath << std::endl;
            return false;
        }
        
        std::string line;
        while (std::getline(file, line)) {
            // Ignore comments and empty lines
            if (line.empty() || line[0] == '#') {
                continue;
            }

            std::istringstream iss(line);
            std::string key, value;

            // Split line into key and value
            if (std::getline(iss, key, '=') && std::getline(iss, value)) {
                // Remove whitespace from the key and value
                key.erase(key.find_last_not_of(" \t\n\r\f\v") + 1);
                key.erase(0, key.find_first_not_of(" \t\n\r\f\v"));
                value.erase(value.find_last_not_of(" \t\n\r\f\v") + 1);
                value.erase(0, value.find_first_not_of(" \t\n\r\f\v"));

                configMap[key] = value;
            }
        }

        file.close();
        return true;
    }

    // 获取配置值的方法
    std::string getConfig(const std::string& key) const {
        auto it = configMap.find(key);
        if (it != configMap.end()) {
            return it->second;
        }
        return "";
    }

private:
    std::map<std::string, std::string> configMap; // 存储配置键值对
    Configure() {} // 私有构造函数，防止外部直接实例化
    Configure(const Configure&) = delete; // 禁止拷贝构造
    Configure& operator=(const Configure&) = delete; // 禁止赋值操作
};

class ThreadPool {
public:
    ThreadPool(size_t numThreads = 30);
    ~ThreadPool();
    void enqueue(std::function<void()> task);

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex queueMutex;
    std::condition_variable condition;
    bool stop;
};




std::string generateRandomSuffix();
void InitLog();

} // namespace common
} // namespace intercept