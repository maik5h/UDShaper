// Code to log messages is defined here.
// CLAP offers a logging feature but it is not supported by the host I am using (FL Studio). Therefore
// messages are logged to a file on my Desktop.

#pragma once

#include <fstream>
#include <iostream>

struct Logger {
    // Indicates whether the plugin is in debug mode. Effectively deactivates logToFile(), which should never
    // be used outside debugging as it is just a temporary solution.
    static bool debugMode;

    // Logs the message to the file log.txt on my Desktop.
    static void logToFile(const std::string& message);
};
