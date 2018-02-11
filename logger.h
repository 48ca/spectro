#include <chrono>
#include <iomanip>
#include <ctime>
#include <vector>
#include <mutex>
#include <string>

class Logger {
  private:
    std::vector<std::string> messages;
    std::mutex mutex;

  public:
    Logger() { };
    ~Logger(void) { };

    bool verbose = true; // default to true

    void log(std::string msg, std::ostream& out = std::cout) {
        std::lock_guard<std::mutex> lock(mutex);
        messages.emplace_back(msg);

        if(verbose) {
            auto now = std::chrono::system_clock::now();
            std::time_t now_c = std::chrono::system_clock::to_time_t(now);
            auto timestr = std::put_time(std::localtime(&now_c), "%T");

            out << '[' << timestr << "] " << msg << '\n';
        }
    };
    void error(const std::string& msg) {
        log(msg, std::cerr);
    };
};
