#pragma once

#include "pnpl/inference_runner.hpp"
#include <string>
#include <filesystem>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <unordered_set>

namespace pnpl {

    class InferenceMonitor {
    public:
        InferenceMonitor(const std::string& modelPath,
                        const std::string& inputDir = "data/input",
                        const std::string& outputDir = "data/output",
                        int numWorkers = 1);
        ~InferenceMonitor();

        // Start monitoring and processing
        bool start();

        // Stop monitoring gracefully
        void stop();

        // Get current status
        std::string getStatus() const;

        // Get number of jobs in queue/processing
        int getQueueSize() const;

    private:
        std::string modelPath_;
        std::string inputDirectory_;
        std::string outputDirectory_;
        std::string processingDirectory_;  // NEW: Directory for files being processed
        int numWorkers_;

        // Thread management
        std::atomic<bool> running_{false};
        std::vector<std::thread> workers_;
        std::thread monitorThread_;

        // Job queue and synchronization
        std::queue<std::string> jobQueue_;
        mutable std::mutex queueMutex_;
        std::condition_variable jobCondition_;

        // Monitoring thread function
        void monitorDirectory();

        // Worker thread function
        void workerFunction(int workerId);

        // Process any existing files in processing directory (recovery)
        void processExistingFiles();

        // Process a single file
        bool processFile(const std::string& jobId,
                       const std::filesystem::path& inputPath,
                       const std::filesystem::path& outputPath);

        // Update job status (for future use)
        void updateJobStatus(const std::string& jobId,
                            const std::string& status,
                            const std::string& message = "");
    };

} // namespace pnpl