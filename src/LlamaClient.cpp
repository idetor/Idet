
#include "LlamaClient.hpp"
#include <iostream>
#include <thread>

LlamaClient::LlamaClient(LogCallback logger) : debug_logger(logger) {
    llama_backend_init();
    log("Llama backend initialized.");
}

LlamaClient::~LlamaClient() {
    llama_backend_free();
}

void LlamaClient::log(const std::string& msg) {
    if (debug_logger) debug_logger("[LlamaClient] " + msg);
}

bool LlamaClient::load_model(const std::string& model_path, int n_ctx) {
    last_error = "";

    // CRITICAL: Fix for the GGML_ASSERT crash
    if (model_path.empty()) {
        last_error = "Model path is empty!";
        log(last_error);
        return false;
    }

    log("Attempting to load: " + model_path);

    //auto m_params = llama_model_default_params();
    //model.reset(llama_model_load_from_file(model_path.c_str(), m_params));
    c_params.n_threads = std::thread::hardware_concurrency();

    if (!model) {
        last_error = "Failed to load model file (check path/permissions)";
        log(last_error);
        return false;
    }

    auto c_params = llama_context_default_params();
    c_params.n_ctx = n_ctx;
    ctx.reset(llama_init_from_model(model.get(), c_params));

    if (!ctx) {
        last_error = "Failed to create context.";
        log(last_error);
        return false;
    }

    log("Model loaded successfully into context.");
    return true;
}

// ... tokenize and token_to_piece remain the same as previous fix ...

std::string LlamaClient::token_to_piece(llama_token token) {
    const struct llama_vocab* vocab = llama_model_get_vocab(model.get());
    
    std::vector<char> result(32);
    // Updated: passing 'vocab' as first argument
    int n_tokens = llama_token_to_piece(vocab, token, result.data(), result.size(), 0, false);
    if (n_tokens < 0) {
        result.resize(-n_tokens);
        llama_token_to_piece(vocab, token, result.data(), result.size(), 0, false);
    } else {
        result.resize(n_tokens);
    }
    return std::string(result.data(), result.size());
}

std::string LlamaClient::complete_text(const std::string& prompt, const GenerationConfig& config) {
    if (!is_ready()) return "";
    auto tokens = tokenize(prompt, true);
    // Logic for inference loop goes here...
    return "Tokens generated: " + std::to_string(tokens.size());
}
std::vector<llama_token> LlamaClient::tokenize(const std::string& text, bool add_bos) {
    const struct llama_vocab* vocab = llama_model_get_vocab(model.get());

    int n_tokens = text.size() + (add_bos ? 1 : 0);
    std::vector<llama_token> tokens(n_tokens);

    // Tokenize
    int result = llama_tokenize(
        vocab,
        text.c_str(),
        text.length(),
        tokens.data(),
        tokens.size(),
        add_bos,
        false  // special tokens
    );

    if (result < 0) {
        tokens.resize(-result);
        result = llama_tokenize(
            vocab,
            text.c_str(),
            text.length(),
            tokens.data(),
            tokens.size(),
            add_bos,
            false
        );
    }

    tokens.resize(result);
    return tokens;
}