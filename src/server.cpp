#include "pnpl/inference_monitor.hpp"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <csignal>
#include <filesystem>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#elif defined(__linux__)
#include <unistd.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

// Global signal handler
std::atomic<bool> g_running(true);

void signalHandler(int signal) {
    std::cout << "Received signal " << signal << ", shutting down..." << std::endl;
    g_running = false;
}

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
        path = std::filesystem::current_path().string() + "/pnpl_server";
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
    std::cout << "PNPL Server: Monitor and process jobs" << std::endl;
    std::cout << "Usage: " << program << " <model_path> [options]" << std::endl;
    std::cout << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "  --workers <n>        Number of worker threads (default: 1)" << std::endl;
    std::cout << "  --input-dir <dir>    Input directory (default: <project>/data/input)" << std::endl;
    std::cout << "  --output-dir <dir>   Output directory (default: <project>/data/output)" << std::endl;
    std::cout << std::endl;
    std::cout << "Note: If input/output dirs are relative, they're relative to project root" << std::endl;
}

int main(int argc, char* argv[]) {
    // Check for model path
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::string modelPath = argv[1];

    // Get project root for default paths
    std::string projectRoot = getProjectRoot();
    std::string inputDir = projectRoot + "/data/input";
    std::string outputDir = projectRoot + "/data/output";
    int numWorkers = 1;

    // Parse options
    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--workers" && i + 1 < argc) {
            try {
                numWorkers = std::stoi(argv[++i]);
                if (numWorkers < 1) numWorkers = 1;
            } catch (...) {
                std::cerr << "Invalid worker count, using default" << std::endl;
            }
        }
        else if (arg == "--input-dir" && i + 1 < argc) {
            std::string dir = argv[++i];
            // If relative path, resolve it relative to current working directory
            if (dir[0] != '/') {
                inputDir = std::filesystem::absolute(dir);
            } else {
                inputDir = dir;
            }
        }
        else if (arg == "--output-dir" && i + 1 < argc) {
            std::string dir = argv[++i];
            // If relative path, resolve it relative to current working directory
            if (dir[0] != '/') {
                outputDir = std::filesystem::absolute(dir);
            } else {
                outputDir = dir;
            }
        }
        else if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        }
    }

    // Ensure default paths are absolute
    inputDir = std::filesystem::absolute(inputDir);
    outputDir = std::filesystem::absolute(outputDir);

    // Check if model file exists
    if (!std::filesystem::exists(modelPath)) {
        std::cerr << "Error: Model file not found: " << modelPath << std::endl;
        return 1;
    }

    // Set up signal handler for graceful shutdown
    std::signal(SIGINT, signalHandler);

    // Create directories if they don't exist
    try {
        std::filesystem::create_directories(inputDir);
        std::filesystem::create_directories(outputDir);
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Error creating directories: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "Starting PNPL inference server..." << std::endl;
    std::cout << "Project root: " << projectRoot << std::endl;
    std::cout << "Model: " << modelPath << std::endl;
    std::cout << "Input directory: " << inputDir << std::endl;
    std::cout << "Output directory: " << outputDir << std::endl;
    std::cout << "Worker threads: " << numWorkers << std::endl;

    // Initialize and start inference monitor
    pnpl::InferenceMonitor monitor(modelPath, inputDir, outputDir, numWorkers);

    if (!monitor.start()) {
        std::cerr << "Failed to start inference monitor" << std::endl;
        return 1;
    }

    std::cout << "Server started. Press Ctrl+C to stop." << std::endl;

    // Main loop - periodically display status
    while (g_running) {
        std::cout << "Queue size: " << monitor.getQueueSize() << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    // Graceful shutdown
    std::cout << "Shutting down server..." << std::endl;
    monitor.stop();
    std::cout << "Server stopped" << std::endl;

    return 0;
}