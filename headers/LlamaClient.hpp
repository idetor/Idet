#ifndef LLAMA_CLIENT_HPP
#define LLAMA_CLIENT_HPP

#include <string>
#include <vector>
#include <memory>
#include "llama.h"

struct GenerationConfig {
    int32_t n_predict   = 128;
    float   temp        = 0.80f;
    int32_t top_k       = 40;
    float   top_p       = 0.95f;
    int32_t n_threads   = 4; // Added for performance control
};

// Custom deleters for RAII-style memory management
struct LlamaModelDeleter { 
    void operator()(llama_model* m) const { llama_model_free(m); } 
};
struct LlamaContextDeleter { void operator()(llama_context* c) const { llama_free(c); } };

class LlamaClient {
public:
    LlamaClient();
    ~LlamaClient();

    // Deleted copy operations to prevent double-free
    LlamaClient(const LlamaClient&) = delete;
    LlamaClient& operator=(const LlamaClient&) = delete;

    // Support move operations
    LlamaClient(LlamaClient&&) noexcept = default;
    LlamaClient& operator=(LlamaClient&&) noexcept = default;

    /**
     * @brief Loads the GGUF model and initializes the context.
     */
    bool load_model(const std::string& model_path, int n_ctx = 2048);

    /**
     * @brief Generates text based on a prompt.
     */
    std::string complete_text(const std::string& prompt, const GenerationConfig& config = {});

    bool is_ready() const { return model != nullptr && ctx != nullptr; }

private:
    // Using unique_ptr ensures memory is freed even if an exception occurs
    std::unique_ptr<llama_model, LlamaModelDeleter> model;
    std::unique_ptr<llama_context, LlamaContextDeleter> ctx;

    std::vector<llama_token> tokenize(const std::string& text, bool add_special);
    std::string token_to_piece(llama_token token);
};

#endif // LLAMA_CLIENT_HPP