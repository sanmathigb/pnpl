#pragma once

#include <string>
#include <vector>
#include <optional>
#include <filesystem>

namespace pnpl {

    // Result structure for job outputs
    struct JobResult {
        std::string id;
        std::string outputText;
        bool success;
        std::string errorMessage;
    };

    class PopManager {
    public:
        PopManager(const std::string& outputDirectory = "data/output");
        ~PopManager() = default;

        // Get result for a specific job ID
        std::optional<JobResult> popResult(const std::string& jobId);

        // Get the most recent result
        std::optional<JobResult> popLatest();

        // List all completed jobs
        std::vector<std::string> listCompleted() const;

        // Check if a job is completed
        bool isJobCompleted(const std::string& jobId) const;

        // Check if a job exists (even if not completed)
        bool jobExists(const std::string& jobId) const;

    private:
        std::string resultsDirectory_;

        // Extract job ID from filename
        std::string extractJobId(const std::string& filename) const;

        // Read result file
        bool readResultFile(const std::filesystem::path& path, std::string& content) const;
    };

} // namespace pnpl