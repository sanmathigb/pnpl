#pragma once

#include <string>
#include <filesystem>

// Forward declarations for llama.cpp types
struct llama_model;
struct llama_context;

namespace pnpl {

    class InferenceRunner {
    public:
        InferenceRunner();
        ~InferenceRunner();

        // Initialize with a model path
        bool init(const std::string& model_path);

        // Run inference on string input/output
        bool run(const std::string& input, std::string& output);

        // Run inference on input file, write to output file
        bool runOnFile(const std::filesystem::path& input_path,
                      const std::filesystem::path& output_path);

        // Check if model is initialized
        bool isInitialized() const;

        // Get last error message
        std::string getLastError() const;

    private:
        // Model is loaded once and reused
        llama_model* model_ = nullptr;

        // Context is created fresh for each run (like simple.cpp)
        llama_context* ctx_ = nullptr;

        std::string lastError_;
        static const int DEFAULT_CTX_SIZE = 2048;

        // Set error message
        void setError(const std::string& error);

        // Simple prompt formatting
        std::string formatPrompt(const std::string& input);
    };

} // namespace pnpl