#include "pnpl/push_manager.hpp"
#include "pnpl/pop_manager.hpp"
#include "pnpl/inference_runner.hpp"

#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <algorithm>
#include <iomanip>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#elif defined(__linux__)
#include <unistd.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

std::string getExecutablePath() {
    std::string path;

#ifdef __APPLE__
    char buffer[PATH_MAX];
    uint32_t size = sizeof(buffer);
    if (_NSGetExecutablePath(buffer, &size) == 0) {
        path = std::filesystem::canonical(buffer).string();
    }
#elif defined(__linux__)
    char buffer[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (len != -1) {
        buffer[len] = '\0';
        path = std::filesystem::canonical(buffer).string();
    }
#elif defined(_WIN32)
    char buffer[MAX_PATH];
    if (GetModuleFileNameA(NULL, buffer, MAX_PATH) > 0) {
        path = std::filesystem::canonical(buffer).string();
    }
#endif

    if (path.empty()) {
        // Fallback: use current working directory
        path = std::filesystem::current_path().string() + "/pnpl";
        std::cerr << "Warning: Could not determine executable path, using fallback" << std::endl;
    }

    return path;
}

std::string getProjectRoot() {
    std::filesystem::path exePath = getExecutablePath();
    std::filesystem::path buildDir = exePath.parent_path();

    // Go up one level to project root (assuming we're in build/)
    std::filesystem::path projectRoot = buildDir.parent_path();

    return projectRoot.string();
}

void printUsage(const char* program) {
    std::cout << "PNPL: Push Now, Pop Later" << std::endl;
    std::cout << "Usage: " << program << " <command> [options]" << std::endl;
    std::cout << std::endl;
    std::cout << "Commands:" << std::endl;
    std::cout << "  push <content>       Create a new job with the given content" << std::endl;
    std::cout << "  push --file <path>   Create a new job from file content" << std::endl;
    std::cout << "  pop [job_id]         Get results for a job (defaults to latest)" << std::endl;
    std::cout << "  list                 List all available jobs" << std::endl;
    std::cout << "  status <job_id>      Check status of a job" << std::endl;
    std::cout << std::endl;
    std::cout << "Data directory: " << getProjectRoot() << "/data" << std::endl;
}

int main(int argc, char* argv[]) {
    // If no command is provided, print usage
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    // Setup directories - use project root, not build directory
    std::string projectRoot = getProjectRoot();
    std::string dataDir = projectRoot + "/data";
    std::string inputDir = dataDir + "/input";
    std::string outputDir = dataDir + "/output";

    // Ensure directories exist
    try {
        std::filesystem::create_directories(inputDir);
        std::filesystem::create_directories(outputDir);
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error creating directories: " << e.what() << std::endl;
        return 1;
    }

    std::string command = argv[1];

    // Handle push command
    if (command == "push") {
        if (argc < 3) {
            std::cerr << "Error: 'push' requires content or --file option" << std::endl;
            return 1;
        }

        std::string content;
        bool is_file = false;

        // Check if using --file option
        if (std::string(argv[2]) == "--file") {
            if (argc < 4) {
                std::cerr << "Error: --file option requires a path" << std::endl;
                return 1;
            }
            is_file = true;
            std::string file_path = argv[3];

            // Check if file exists
            if (!std::filesystem::exists(file_path)) {
                std::cerr << "Error: File not found: " << file_path << std::endl;
                return 1;
            }

            // Read file content
            std::ifstream file(file_path);
            if (!file) {
                std::cerr << "Error: Failed to open file: " << file_path << std::endl;
                return 1;
            }

            content = std::string((std::istreambuf_iterator<char>(file)),
                                 std::istreambuf_iterator<char>());
        } else {
            // Use direct content from command line
            content = argv[2];
        }

        // Create the job using explicit input directory
        pnpl::PushManager pushManager(inputDir);
        std::string jobId = pushManager.createJob(content);

        if (jobId.empty()) {
            std::cerr << "Error: Failed to create job" << std::endl;
            return 1;
        }

        std::cout << "Job created with ID: " << jobId << std::endl;
        std::cout << "Input directory: " << inputDir << std::endl;
        return 0;
    }
    // Handle pop command
    else if (command == "pop") {
        pnpl::PopManager popManager(outputDir);

        // Check if job ID is provided
        if (argc >= 3) {
            std::string jobId = argv[2];
            auto result = popManager.popResult(jobId);

            if (!result) {
                std::cerr << "Error: Job " << jobId << " not found or not completed" << std::endl;
                return 1;
            }

            if (!result->success) {
                std::cerr << "Error: " << result->errorMessage << std::endl;
                return 1;
            }

            // Output the result
            std::cout << result->outputText << std::endl;
        } else {
            // Get latest result
            auto result = popManager.popLatest();

            if (!result) {
                std::cerr << "Error: No completed jobs found" << std::endl;
                return 1;
            }

            if (!result->success) {
                std::cerr << "Error: " << result->errorMessage << std::endl;
                return 1;
            }

            // Output the result
            std::cout << "Latest job: " << result->id << std::endl;
            std::cout << "-----------------------------------" << std::endl;
            std::cout << result->outputText << std::endl;
        }

        return 0;
    }
    // Handle list command
    else if (command == "list") {
        pnpl::PushManager pushManager(inputDir);
        pnpl::PopManager popManager(outputDir);

        // Get all jobs
        auto jobs = pushManager.listJobs();

        if (jobs.empty()) {
            std::cout << "No jobs found" << std::endl;
            return 0;
        }

        // Sort jobs by ID (which includes timestamp)
        std::sort(jobs.begin(), jobs.end());

        // Display jobs
        std::cout << "Available jobs:" << std::endl;
        std::cout << std::left << std::setw(30) << "Job ID" << "Status" << std::endl;
        std::cout << std::string(50, '-') << std::endl;

        for (const auto& jobId : jobs) {
            std::string status = popManager.isJobCompleted(jobId) ? "Completed" : "Pending";
            std::cout << std::left << std::setw(30) << jobId << status << std::endl;
        }

        return 0;
    }
    // Handle status command
    else if (command == "status") {
        if (argc < 3) {
            std::cerr << "Error: 'status' requires a job ID" << std::endl;
            return 1;
        }

        std::string jobId = argv[2];

        pnpl::PushManager pushManager(inputDir);
        pnpl::PopManager popManager(outputDir);

        // Check if job exists
        if (!popManager.jobExists(jobId)) {
            std::cerr << "Error: Job " << jobId << " not found" << std::endl;
            return 1;
        }

        // Display status
        std::cout << "Job ID: " << jobId << std::endl;

        if (popManager.isJobCompleted(jobId)) {
            std::cout << "Status: Completed" << std::endl;
            std::cout << "Result is available. Use 'pop " << jobId << "' to view." << std::endl;
        } else {
            std::cout << "Status: Pending" << std::endl;
            std::cout << "Job is waiting to be processed." << std::endl;
        }

        return 0;
    }
    // Handle help command
    else if (command == "help" || command == "--help" || command == "-h") {
        printUsage(argv[0]);
        return 0;
    }
    // Unknown command
    else {
        std::cerr << "Unknown command: " << command << std::endl;
        printUsage(argv[0]);
        return 1;
    }
}