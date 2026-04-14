#ifndef LLAMA_CLIENT_HPP
#define LLAMA_CLIENT_HPP

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include "llama.h"

struct GenerationConfig {
    int32_t n_predict   = 128;
    float   temp        = 0.80f;
    int32_t top_k       = 40;
    float   top_p       = 0.95f;
    int32_t n_threads   = 4;
};

struct LlamaModelDeleter {
    void operator()(llama_model* m) const { llama_model_free(m); }
};

struct LlamaContextDeleter {
    void operator()(llama_context* c) const { llama_free(c); }
};

class LlamaClient {
public:
    using LogCallback = std::function<void(const std::string&)>;

    LlamaClient(LogCallback logger = nullptr);
    ~LlamaClient();

    LlamaClient(const LlamaClient&) = delete;
    LlamaClient& operator=(const LlamaClient&) = delete;
    LlamaClient(LlamaClient&&) noexcept = default;
    LlamaClient& operator=(LlamaClient&&) noexcept = default;

    // ✅ ONLY DECLARATION HERE
    bool load_model(const std::string& model_path, int n_ctx = 2048);

    std::string complete_text(const std::string& prompt,
                              const GenerationConfig& config = {});

    bool is_ready() const { return model != nullptr && ctx != nullptr; }
    std::string get_last_error() const { return last_error; }

private:
    void log(const std::string& msg);

    std::string last_error;
    LogCallback debug_logger;

    std::unique_ptr<llama_model, LlamaModelDeleter> model;
    std::unique_ptr<llama_context, LlamaContextDeleter> ctx;

    std::vector<llama_token> tokenize(const std::string& text, bool add_special);
    std::string token_to_piece(llama_token token);
};
// Ignaz hat einen Igel am gewissen, was ein fahrrad mörder.
#endif