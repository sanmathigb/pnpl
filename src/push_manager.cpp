#include "pnpl/push_manager.hpp"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <iostream>

namespace pnpl {

PushManager::PushManager(const std::string& inputDirectory)
    : inputDirectory_(inputDirectory), counterFile_(inputDirectory + "/.counter") {

    // Create input directory if it doesn't exist
    if (!std::filesystem::exists(inputDirectory_)) {
        std::filesystem::create_directories(inputDirectory_);
    }
}

std::string PushManager::createJob(const std::string& content) {
    if (content.empty()) {
        std::cerr << "Cannot create job with empty content" << std::endl;
        return "";
    }

    // Generate a unique job ID
    std::string jobId = generateJobID();

    // Create the input file
    std::filesystem::path filePath = std::filesystem::path(inputDirectory_) / (jobId + ".txt");

    if (!writeToFile(filePath, content)) {
        std::cerr << "Failed to write job content to file" << std::endl;
        return "";
    }

    return jobId;
}

std::vector<std::string> PushManager::listJobs() const {
    std::vector<std::string> jobs;

    for (const auto& entry : std::filesystem::directory_iterator(inputDirectory_)) {
        if (!entry.is_regular_file()) continue;

        std::string filename = entry.path().filename().string();

        // Skip the counter file
        if (filename == ".counter") continue;

        // Extract job ID (remove .txt extension)
        if (filename.size() > 4 && filename.substr(filename.size() - 4) == ".txt") {
            std::string jobId = filename.substr(0, filename.size() - 4);
            jobs.push_back(jobId);
        }
    }

    return jobs;
}

int PushManager::loadCounter() const {
    std::lock_guard<std::mutex> lock(counterMutex_);

    // Try to read counter from file
    std::ifstream file(counterFile_);
    if (file) {
        int counter = 0;
        file >> counter;
        return counter;
    }

    // If file doesn't exist or can't be read, start from 1
    return 1;
}

void PushManager::saveCounter(int counter) const {
    std::lock_guard<std::mutex> lock(counterMutex_);

    std::ofstream file(counterFile_);
    if (!file) {
        std::cerr << "Failed to save counter to " << counterFile_ << std::endl;
        return;
    }

    file << counter;
}

std::string PushManager::generateJobID() {
    int counter = loadCounter();

    // Get current time
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);

    // Format with timestamp and sequence number
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t_now), "%Y%m%d%H%M%S")
       << "_" << std::setw(6) << std::setfill('0') << counter;

    // Increment counter for next job
    saveCounter(counter + 1);

    return ss.str();
}

bool PushManager::writeToFile(const std::filesystem::path& filePath,
                             const std::string& content) const {
    std::ofstream file(filePath);
    if (!file) {
        return false;
    }

    file << content;
    return !file.fail();
}

} // namespace pnpl