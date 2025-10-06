#include "OutputLogger.h"

void OutputLogger::Log(const std::string &msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::cout << msg << std::endl;
}
