#pragma once

#include <string>
#include <filesystem>
#include <mutex>

namespace pnpl {

    class PushManager {
    public:
        // Constructor with default input directory
        PushManager(const std::string& inputDirectory = "data/input");
        ~PushManager() = default;

        // Create a new job with the given content
        // Returns the job ID
        std::string createJob(const std::string& content);

        // List all jobs created by this push manager
        std::vector<std::string> listJobs() const;

    private:
        std::string inputDirectory_;
        std::string counterFile_;

        // Thread safety for ID generation
        mutable std::mutex counterMutex_;

        // Load the current counter value
        int loadCounter() const;

        // Save the counter after incrementing
        void saveCounter(int counter) const;

        // Generate a new job ID
        std::string generateJobID();

        // Write content to a file
        bool writeToFile(const std::filesystem::path& filePath,
                        const std::string& content) const;
    };

} // namespace pnpl