#include "pnpl/pop_manager.hpp"
#include <fstream>
#include <iostream>
#include <algorithm>
#include <iostream>

namespace pnpl {

PopManager::PopManager(const std::string& outputDirectory)
    : resultsDirectory_(outputDirectory) {

    // Create output directory if it doesn't exist
    if (!std::filesystem::exists(resultsDirectory_)) {
        std::filesystem::create_directories(resultsDirectory_);
    }
}

std::optional<JobResult> PopManager::popResult(const std::string& jobId) {
    std::filesystem::path resultPath = std::filesystem::path(resultsDirectory_) / (jobId + ".txt");

    if (!std::filesystem::exists(resultPath)) {
        return std::nullopt;
    }

    std::string content;
    if (!readResultFile(resultPath, content)) {
        return JobResult{jobId, "", false, "Failed to read result file"};
    }

    return JobResult{jobId, content, true, ""};
}

std::optional<JobResult> PopManager::popLatest() {
    auto jobs = listCompleted();

    if (jobs.empty()) {
        return std::nullopt;
    }

    // Find the latest job by modification time
    std::filesystem::path latestPath;
    std::filesystem::file_time_type latestTime;
    bool first = true;

    for (const auto& jobId : jobs) {
        std::filesystem::path path = std::filesystem::path(resultsDirectory_) / (jobId + ".txt");

        if (!std::filesystem::exists(path)) continue;

        auto modTime = std::filesystem::last_write_time(path);

        if (first || modTime > latestTime) {
            latestPath = path;
            latestTime = modTime;
            first = false;
        }
    }

    if (first) {
        return std::nullopt;  // No valid files found
    }

    // Extract the job ID from the path
    std::string jobId = extractJobId(latestPath.filename().string());

    std::string content;
    if (!readResultFile(latestPath, content)) {
        return JobResult{jobId, "", false, "Failed to read result file"};
    }

    return JobResult{jobId, content, true, ""};
}

std::vector<std::string> PopManager::listCompleted() const {
    std::vector<std::string> jobs;

    for (const auto& entry : std::filesystem::directory_iterator(resultsDirectory_)) {
        if (!entry.is_regular_file()) continue;

        std::string filename = entry.path().filename().string();

        // Extract job ID (remove .txt extension)
        if (filename.size() > 4 && filename.substr(filename.size() - 4) == ".txt") {
            std::string jobId = filename.substr(0, filename.size() - 4);
            jobs.push_back(jobId);
        }
    }

    // Sort by job ID (which includes timestamps)
    std::sort(jobs.begin(), jobs.end());

    return jobs;
}

bool PopManager::isJobCompleted(const std::string& jobId) const {
    std::filesystem::path resultPath = std::filesystem::path(resultsDirectory_) / (jobId + ".txt");
    return std::filesystem::exists(resultPath);
}

bool PopManager::jobExists(const std::string& jobId) const {
    // Check if either input or output file exists
    std::filesystem::path inputPath = std::filesystem::path("input") / (jobId + ".txt");
    std::filesystem::path outputPath = std::filesystem::path(resultsDirectory_) / (jobId + ".txt");

    return std::filesystem::exists(inputPath) || std::filesystem::exists(outputPath);
}

std::string PopManager::extractJobId(const std::string& filename) const {
    // Remove .txt extension
    if (filename.size() > 4 && filename.substr(filename.size() - 4) == ".txt") {
        return filename.substr(0, filename.size() - 4);
    }

    return filename;
}

bool PopManager::readResultFile(const std::filesystem::path& path, std::string& content) const {
    std::ifstream file(path);
    if (!file) {
        return false;
    }

    content = std::string((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

    return true;
}

} // namespace pnpl