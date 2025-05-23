#include "pnpl/inference_runner.hpp"
#include "llama.h"
#include <iostream>
#include <fstream>
#include <vector>

namespace pnpl {

InferenceRunner::InferenceRunner() {
    // Load all available backends (GPU, CPU, etc.)
    ggml_backend_load_all();
}

InferenceRunner::~InferenceRunner() {
    if (ctx_) llama_free(ctx_);
    if (model_) llama_model_free(model_);
}

bool InferenceRunner::init(const std::string& model_path) {
    // EXACTLY like simple.cpp - Initialize model parameters
    llama_model_params model_params = llama_model_default_params();
    model_params.n_gpu_layers = 99; // Use GPU

    // Load model - EXACT API from simple.cpp
    model_ = llama_model_load_from_file(model_path.c_str(), model_params);
    if (!model_) {
        setError("Failed to load model from " + model_path);
        return false;
    }

    return true;
}

bool InferenceRunner::run(const std::string& input, std::string& output) {
    if (!model_) {
        setError("Model not initialized");
        return false;
    }

    // Format the prompt - AI sees full context, output starts clean
    std::string formatted_input = formatPrompt(input);

    // Get vocab - EXACT API from simple.cpp
    const llama_vocab* vocab = llama_model_get_vocab(model_);

    // Tokenize - EXACT pattern from simple.cpp
    const int n_prompt = -llama_tokenize(vocab, formatted_input.c_str(), formatted_input.size(), NULL, 0, true, true);
    if (n_prompt <= 0) {
        setError("Failed to tokenize prompt");
        return false;
    }

    // Allocate and tokenize - EXACT pattern from simple.cpp
    std::vector<llama_token> prompt_tokens(n_prompt);
    if (llama_tokenize(vocab, formatted_input.c_str(), formatted_input.size(), prompt_tokens.data(), prompt_tokens.size(), true, true) < 0) {
        setError("Failed to tokenize the prompt");
        return false;
    }

    // AI/ML RESEARCHER INSIGHT: Dynamic context sizing for efficiency
    llama_context_params ctx_params = llama_context_default_params();
    int n_predict = 1500; // Optimized for TinyLlama's sweet spot

    // Dynamic context calculation to maximize model efficiency
    int required_context = n_prompt + n_predict + 100;
    int max_context = 2048; // TinyLlama's architectural limit

    if (required_context > max_context) {
        // Graceful degradation: reduce generation length to fit context
        n_predict = max_context - n_prompt - 100;
        if (n_predict < 200) {
            setError("Input too large for model context window");
            return false;
        }
        std::cerr << "Warning: Reduced generation to " << n_predict << " tokens due to context limits" << std::endl;
    }

    ctx_params.n_ctx = n_prompt + n_predict + 100;
    ctx_params.n_batch = n_prompt;
    ctx_params.no_perf = false; // Enable performance counters like simple.cpp

    // Create context - EXACT API from simple.cpp
    ctx_ = llama_init_from_model(model_, ctx_params);
    if (!ctx_) {
        setError("Failed to create context");
        return false;
    }

    // Initialize sampler - EXACT pattern from simple.cpp
    auto sparams = llama_sampler_chain_default_params();
    sparams.no_perf = false; // Same as simple.cpp
    llama_sampler* smpl = llama_sampler_chain_init(sparams);

    // Add repetition penalty BEFORE greedy sampler
    llama_sampler_chain_add(smpl, llama_sampler_init_penalties(
        64,    // last_n tokens to consider
        1.1f,  // repeat penalty (>1.0 reduces repetition)
        0.0f,  // frequency penalty
        0.0f   // presence penalty
    ));
    // AI/ML INSIGHT: Greedy sampling for deterministic, production-ready output
    llama_sampler_chain_add(smpl, llama_sampler_init_greedy());

    // PROPER ECHO FIX: Start output cleanly, no prompt echo
    output = "";

    // Print prompt tokens like simple.cpp (to debug)
    //for (auto id: prompt_tokens) {
    /*    char buf[128];
        int n = llama_token_to_piece(vocab, id, buf, sizeof(buf), 0, true);
        if (n < 0) {
            setError("Failed to convert prompt token to piece");
            llama_sampler_free(smpl);
            llama_free(ctx_);
            ctx_ = nullptr;
            return false;
        }
        // Add to output for debugging
        output += std::string(buf, n);
    }*/

    // Prepare batch - EXACT pattern from simple.cpp
    llama_batch batch = llama_batch_get_one(prompt_tokens.data(), prompt_tokens.size());

    // Main generation loop - EXACT pattern from simple.cpp
    int n_decode = 0;
    llama_token new_token_id;

    for (int n_pos = 0; n_pos + batch.n_tokens < n_prompt + n_predict; ) {
        // Evaluate batch - EXACT API from simple.cpp
        if (llama_decode(ctx_, batch)) {
            setError("Failed to eval batch");
            llama_sampler_free(smpl);
            llama_free(ctx_);
            ctx_ = nullptr;
            return false;
        }

        n_pos += batch.n_tokens;

        // Sample next token - EXACT pattern from simple.cpp
        new_token_id = llama_sampler_sample(smpl, ctx_, -1);

        // Check for end of generation - EXACT pattern from simple.cpp
        if (llama_vocab_is_eog(vocab, new_token_id)) {
            break;
        }

        // Convert token to text - EXACT pattern from simple.cpp
        char buf[128];
        int n = llama_token_to_piece(vocab, new_token_id, buf, sizeof(buf), 0, true);
        if (n < 0) {
            setError("Failed to convert token to piece");
            llama_sampler_free(smpl);
            llama_free(ctx_);
            ctx_ = nullptr;
            return false;
        }
        output += std::string(buf, n);

        // Prepare next batch - EXACT pattern from simple.cpp
        batch = llama_batch_get_one(&new_token_id, 1);
        n_decode += 1;
    }

    // Cleanup - EXACT pattern from simple.cpp
    llama_sampler_free(smpl);
    llama_free(ctx_);
    ctx_ = nullptr;

    return true;
}

std::string InferenceRunner::formatPrompt(const std::string& input) {
    // AI/ML RESEARCHER APPROACH: Optimal prompt engineering for small models

    // For large code files - include full context but guide output structure
    if (input.length() > 500) {
        return "You are a senior software engineer conducting a code review. "
               "Analyze the following C++ code and provide a comprehensive technical analysis.\n\n"
               "CODE TO ANALYZE:\n" + input + "\n\n"
               "TECHNICAL ANALYSIS:\n"
               "1. Purpose: What does this code accomplish?\n"
               "2. Architecture: Key classes, methods, and design patterns\n"
               "3. Implementation: Notable technical details and algorithms\n"
               "4. Quality: Code quality, best practices, potential improvements\n"
               "5. Usage: How this code fits into a larger system\n\n"
               "Provide detailed analysis:\n\n";
    }

    // For code snippets - focused technical analysis
    if (input.find("```") != std::string::npos ||
        input.find("#include") != std::string::npos ||
        input.find("class ") != std::string::npos) {
        return "Analyze this C++ code and explain its technical implementation:\n\n"
               + input + "\n\n"
               "Technical Analysis:\n"
               "- Purpose and functionality\n"
               "- Key components and algorithms\n"
               "- Design patterns and best practices\n"
               "- Performance considerations\n\n"
               "Detailed explanation:\n\n";
    }

    // For technical explanations - structured educational format
    if (input.find("Explain") == 0 || input.find("What") == 0) {
        return "Technical Question: " + input + "\n\n"
               "Provide a comprehensive technical explanation with:\n"
               "1. Clear concept definitions\n"
               "2. Practical C++ code examples\n"
               "3. Real-world usage scenarios\n"
               "4. Best practices and common pitfalls\n\n"
               "Technical Answer:\n\n";
    }

    // For comprehensive guides - structured technical writing
    if (input.find("comprehensive") != std::string::npos ||
        input.find("guide") != std::string::npos) {
        return "Create a comprehensive technical guide: " + input + "\n\n"
               "Structure your guide with:\n"
               "1. Core concepts and definitions\n"
               "2. Detailed code examples with explanations\n"
               "3. Practical implementation patterns\n"
               "4. Performance considerations and best practices\n"
               "5. Common pitfalls and how to avoid them\n\n"
               "Technical Guide:\n\n";
    }

    // Default - clean technical analysis
    return "Technical Request: " + input + "\n\n"
           "Provide a detailed technical response with examples and practical guidance:\n\n";
}

bool InferenceRunner::runOnFile(const std::filesystem::path& input_path,
                              const std::filesystem::path& output_path) {
    if (!std::filesystem::exists(input_path)) {
        setError("Input file not found: " + input_path.string());
        return false;
    }

    std::ifstream in_file(input_path);
    if (!in_file) {
        setError("Failed to open input file: " + input_path.string());
        return false;
    }

    std::string input((std::istreambuf_iterator<char>(in_file)),
                     std::istreambuf_iterator<char>());
    in_file.close();

    std::string output;
    if (!run(input, output)) {
        return false;
    }

    std::filesystem::create_directories(output_path.parent_path());

    std::ofstream out_file(output_path);
    if (!out_file) {
        setError("Failed to create output file: " + output_path.string());
        return false;
    }

    out_file << output;
    out_file.close();

    return true;
}

bool InferenceRunner::isInitialized() const {
    return model_ != nullptr;
}

std::string InferenceRunner::getLastError() const {
    return lastError_;
}

void InferenceRunner::setError(const std::string& error) {
    lastError_ = error;
    std::cerr << "InferenceRunner error: " << error << std::endl;
}

} // namespace pnpl