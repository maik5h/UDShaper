#include "logging.h"

// TODO: This value defaults to true since the plugin is in development. It should be set to false
// or be removed completely at a later stage.
bool Logger::debugMode = true;

void Logger::logToFile(const std::string& message) {
    if (!debugMode) return;

    std::ofstream logFile("C:/Users/mm/Desktop/log.txt", std::ios_base::app);
    if (logFile.is_open()) {
        logFile << message << std::endl;
    } else {
        std::cerr << "Failed to open log file!" << std::endl;
    }
}