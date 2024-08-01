#include <random>

#include "common.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"



namespace intercept{
namespace common {
void dummy() {

}


// Constructor starts the timer
Timer::Timer() : m_startTimePoint(std::chrono::high_resolution_clock::now()) {}

Timer::Timer(const std::string& message) : m_message(message),
    m_startTimePoint(std::chrono::high_resolution_clock::now()) {}

// Destructor prints the elapsed time if the timer hasn't been stopped manually
Timer::~Timer() {
    if (!m_stopped) {
        Stop();
    }
    // std::cout << m_message <<  "  Elapsed time: " << m_elapsedTime << " ms" << std::endl;
}

// Method to stop the timer and return the elapsed time in milliseconds
void Timer::Stop() {
    if (!m_stopped) {
        auto endTimePoint = std::chrono::high_resolution_clock::now();
        auto start = std::chrono::time_point_cast<std::chrono::microseconds>(m_startTimePoint).time_since_epoch().count();
        auto end = std::chrono::time_point_cast<std::chrono::microseconds>(endTimePoint).time_since_epoch().count();

        m_elapsedTime = end - start;
        m_stopped = true;
        if (m_elapsedTime > 0) {
            // std::cout << m_message <<  ", Elapsed time: " << m_elapsedTime << " us" << std::endl;
            spdlog::warn("{}, Elapsed time: {} us ", m_message, m_elapsedTime);
        }
    }
}

// Method to get the elapsed time in microseconds
long long Timer::ElapsedMicroseconds() const {
    return m_elapsedTime;
}

// Method to restart the timer
void Timer::Restart() {
    m_startTimePoint = std::chrono::high_resolution_clock::now();
    m_stopped = false;
}

ThreadPool::ThreadPool(size_t numThreads) : stop(false) {
    for (size_t i = 0; i < numThreads; ++i) {
        workers.emplace_back([this] {
            for (;;) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(this->queueMutex);
                    this->condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); });
                    if (this->stop && this->tasks.empty())
                        return;
                    task = std::move(this->tasks.front());
                    this->tasks.pop();
                }
                task();
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        stop = true;
    }
    condition.notify_all();
    for (std::thread &worker : workers)
        worker.join();
}

void ThreadPool::enqueue(std::function<void()> task) {
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        if (stop)
            throw std::runtime_error("enqueue on stopped ThreadPool");
        tasks.emplace(task);
    }
    condition.notify_one();
}


std::string generateRandomSuffix() {
    // 使用当前时间作为随机数生成器的种子，以获得更好的随机性
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::default_random_engine generator(seed);
    std::uniform_int_distribution<int> distribution(0, 25); // 生成0-25之间的整数，对应字母'a'到'z'

    std::string suffix;
    suffix.reserve(5); // 假设我们想要生成5个随机字符的后缀
    for (size_t i = 0; i < 5; ++i) {
        suffix += static_cast<char>('a' + distribution(generator));
    }
    return suffix;
}

std::atomic<bool> running(true);
void UpdateLogLevelPeriodically(int intervalSeconds) {
    auto& config = Configure::getInstance();
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(intervalSeconds));
        // std::cout << "reload the config: " << CONFIG_FILE << std::endl;
        config.loadConfig(CONFIG_FILE);  // Assuming this reloads the configuration
        std::string loglevel = config.getConfig("loglevel");
        if (loglevel == "debug") {
            spdlog::set_level(spdlog::level::debug);        
        } else if (loglevel == "warning") {
            spdlog::set_level(spdlog::level::warn);
        } else if (loglevel == "info") {
            spdlog::set_level(spdlog::level::info);         
        } else if (loglevel == "error") {
            spdlog::set_level(spdlog::level::err);         
        } else {
            std::cerr << "Invalid log level specified in the config file" << std::endl;
        }        
    }
}
void InitLog() {
    const auto& config = Configure::getInstance();
    std::string pid = std::to_string((long)getpid());
    std::string logpath = config.getConfig("logpath") + "." + pid;
    std::string loglevel = config.getConfig("loglevel");
    try
    {
        std::shared_ptr<spdlog::logger> logger;
        std::string printtype = config.getConfig("logprinttype");
        if (printtype == "console") {
            logger = spdlog::stdout_color_mt("console");
        } else {
            logger = spdlog::basic_logger_mt("basic_logger", logpath);
        }
        spdlog::set_default_logger(logger);
        if (loglevel == "debug") {
            spdlog::set_level(spdlog::level::debug);        
        }
        else if (loglevel == "warning") {
            spdlog::set_level(spdlog::level::warn);
        }
        else if (loglevel == "info") {
            spdlog::set_level(spdlog::level::info);         
        }
        else if (loglevel == "error") {
            spdlog::set_level(spdlog::level::err);         
        }
        else {
            std::cerr << "Invalid log level specified in the config file" << std::endl;
        }
        //spdlog::set_pattern("[%H:%M:%S %z] [%n] [%^---%L---%$] [thread %t] %v");
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] [pid %P tid %t] %v");
        spdlog::flush_every(std::chrono::seconds(5));
        // Start the periodic log level updater thread
        std::thread updateThread(UpdateLogLevelPeriodically, 5); // Check every 60 seconds
        updateThread.detach(); // Detach the thread so it runs independently
    }
    catch (const spdlog::spdlog_ex &ex) {
        std::cout << "Log init failed: " << ex.what() << std::endl;
    }
}

} // namespace common
} // namespace intercept
