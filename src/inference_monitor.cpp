#include "pnpl/inference_monitor.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <algorithm>

namespace pnpl {

InferenceMonitor::InferenceMonitor(const std::string& modelPath,
                                 const std::string& inputDir,
                                 const std::string& outputDir,
                                 int numWorkers)
    : modelPath_(modelPath),
      inputDirectory_(inputDir),
      outputDirectory_(outputDir),
      processingDirectory_(inputDir + "_processing"),
      numWorkers_(numWorkers) {

    // Ensure directories exist
    std::filesystem::create_directories(inputDirectory_);
    std::filesystem::create_directories(outputDirectory_);
    std::filesystem::create_directories(processingDirectory_);
}

InferenceMonitor::~InferenceMonitor() {
    // Ensure we stop everything cleanly
    stop();
}

bool InferenceMonitor::start() {
    // Don't start if already running
    if (running_) return true;

    running_ = true;

    // Process any existing files in the processing directory first
    processExistingFiles();

    // Start the monitor thread
    monitorThread_ = std::thread(&InferenceMonitor::monitorDirectory, this);

    // Start worker threads
    for (int i = 0; i < numWorkers_; ++i) {
        workers_.emplace_back(&InferenceMonitor::workerFunction, this, i);
    }

    std::cout << "Inference monitor started with " << numWorkers_ << " workers" << std::endl;
    std::cout << "Input directory: " << inputDirectory_ << std::endl;
    std::cout << "Processing directory: " << processingDirectory_ << std::endl;
    std::cout << "Output directory: " << outputDirectory_ << std::endl;

    return true;
}

void InferenceMonitor::stop() {
    // Signal all threads to stop
    running_ = false;

    // Wake up any waiting worker threads
    jobCondition_.notify_all();

    // Wait for all threads to finish
    if (monitorThread_.joinable()) {
        monitorThread_.join();
    }

    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }

    workers_.clear();
}

std::string InferenceMonitor::getStatus() const {
    std::stringstream ss;
    ss << "Active workers: " << numWorkers_;
    return ss.str();
}

int InferenceMonitor::getQueueSize() const {
    std::lock_guard<std::mutex> lock(queueMutex_);
    return jobQueue_.size();
}

void InferenceMonitor::processExistingFiles() {
    // Process any files that might be left in the processing directory
    // (in case of a previous unclean shutdown)
    try {
        for (const auto& entry : std::filesystem::directory_iterator(processingDirectory_)) {
            if (!entry.is_regular_file()) continue;

            std::string filename = entry.path().filename().string();

            // Only process .txt files
            if (filename.size() < 4 || filename.substr(filename.size() - 4) != ".txt") {
                continue;
            }

            // Extract job ID from filename
            std::string jobId = filename.substr(0, filename.size() - 4);

            // Add to queue
            {
                std::lock_guard<std::mutex> lock(queueMutex_);
                jobQueue_.push(jobId);
            }

            // Notify one worker
            jobCondition_.notify_one();

            std::cout << "Recovered job from processing directory: " << jobId << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error processing existing files: " << e.what() << std::endl;
    }
}

void InferenceMonitor::monitorDirectory() {
    std::cout << "Directory monitor started" << std::endl;

    // Monitor for new files
    while (running_) {
        try {
            // Scan for new files in input directory
            for (const auto& entry : std::filesystem::directory_iterator(inputDirectory_)) {
                if (!entry.is_regular_file()) continue;

                std::string filename = entry.path().filename().string();

                // Skip counter file
                if (filename == ".counter") continue;

                // Only process .txt files
                if (filename.size() < 4 || filename.substr(filename.size() - 4) != ".txt") {
                    continue;
                }

                // Extract job ID from filename
                std::string jobId = filename.substr(0, filename.size() - 4);

                // Move file from input to processing directory
                std::filesystem::path inputPath = std::filesystem::path(inputDirectory_) / filename;
                std::filesystem::path processingPath = std::filesystem::path(processingDirectory_) / filename;

                try {
                    std::filesystem::rename(inputPath, processingPath);
                    std::cout << "Detected new job: " << jobId << " (moved to processing)" << std::endl;

                    // Add to queue
                    {
                        std::lock_guard<std::mutex> lock(queueMutex_);
                        jobQueue_.push(jobId);
                    }

                    // Notify one worker
                    jobCondition_.notify_one();

                } catch (const std::filesystem::filesystem_error& e) {
                    std::cerr << "Failed to move file " << filename << ": " << e.what() << std::endl;
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "Error scanning directory: " << e.what() << std::endl;
        }

        // Sleep before next scan
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "Directory monitor stopped" << std::endl;
}

void InferenceMonitor::workerFunction(int workerId) {
    std::cout << "Worker " << workerId << " started" << std::endl;

    // Initialize inference runner for this worker
    InferenceRunner runner;

    if (!runner.init(modelPath_)) {
        std::cerr << "Worker " << workerId << " failed to initialize model" << std::endl;
        return;
    }

    std::cout << "Worker " << workerId << " initialized" << std::endl;

    while (running_) {
        std::string jobId;

        // Get a job from the queue
        {
            std::unique_lock<std::mutex> lock(queueMutex_);

            // Wait for a job or stop signal
            jobCondition_.wait(lock, [this] {
                return !running_ || !jobQueue_.empty();
            });

            // Check if we should exit
            if (!running_ && jobQueue_.empty()) {
                break;
            }

            // Get the next job
            if (!jobQueue_.empty()) {
                jobId = jobQueue_.front();
                jobQueue_.pop();
            }
        }

        // Process the job if we got one
        if (!jobId.empty()) {
            // File should be in processing directory
            std::filesystem::path processingPath = std::filesystem::path(processingDirectory_) / (jobId + ".txt");
            std::filesystem::path outputPath = std::filesystem::path(outputDirectory_) / (jobId + ".txt");

            std::cout << "Worker " << workerId << " processing job " << jobId << std::endl;

            if (processFile(jobId, processingPath, outputPath)) {
                std::cout << "Worker " << workerId << " completed job " << jobId << std::endl;

                // Remove the file from processing directory after successful processing
                try {
                    std::filesystem::remove(processingPath);
                    std::cout << "Cleaned up processing file for job " << jobId << std::endl;
                } catch (const std::filesystem::filesystem_error& e) {
                    std::cerr << "Warning: Failed to clean up processing file for job " << jobId << ": " << e.what() << std::endl;
                }
            } else {
                std::cerr << "Worker " << workerId << " failed to process job " << jobId << std::endl;

                // On failure, you might want to move the file back to input directory
                // or to a failed directory for manual inspection
                std::filesystem::path failedPath = std::filesystem::path(inputDirectory_ + "_failed") / (jobId + ".txt");

                try {
                    std::filesystem::create_directories(inputDirectory_ + "_failed");
                    std::filesystem::rename(processingPath, failedPath);
                    std::cout << "Moved failed job " << jobId << " to failed directory" << std::endl;
                } catch (const std::filesystem::filesystem_error& e) {
                    std::cerr << "Warning: Failed to move failed job " << jobId << ": " << e.what() << std::endl;
                }
            }
        }
    }

    std::cout << "Worker " << workerId << " shutting down" << std::endl;
}

bool InferenceMonitor::processFile(const std::string& jobId,
                                 const std::filesystem::path& inputPath,
                                 const std::filesystem::path& outputPath) {
    if (!std::filesystem::exists(inputPath)) {
        std::cerr << "Processing file not found: " << inputPath << std::endl;
        return false;
    }

    // Update status
    updateJobStatus(jobId, "running", "Processing...");

    InferenceRunner runner;

    // Initialize model for this job
    if (!runner.init(modelPath_)) {
        std::cerr << "Failed to initialize model for job " << jobId << std::endl;
        updateJobStatus(jobId, "failed", "Failed to initialize model");
        return false;
    }

    // Process the file
    bool success = runner.runOnFile(inputPath, outputPath);

    if (success) {
        updateJobStatus(jobId, "completed", "Processing completed");
    } else {
        updateJobStatus(jobId, "failed", "Processing failed: " + runner.getLastError());
    }

    return success;
}

void InferenceMonitor::updateJobStatus(const std::string& jobId,
                                     const std::string& status,
                                     const std::string& message) {
    // For MVP, just log to console
    std::cout << "Job " << jobId << " status: " << status;
    if (!message.empty()) {
        std::cout << " - " << message;
    }
    std::cout << std::endl;

    // In future versions, this could update a status file or database
}

} // namespace pnpl