#include "LlamaClient.hpp"
#include <iostream>
#include <vector>

LlamaClient::LlamaClient() {
    llama_backend_init();
}

LlamaClient::~LlamaClient() {
    llama_backend_free();
}

bool LlamaClient::load_model(const std::string& model_path, int n_ctx) {
    auto m_params = llama_model_default_params();
    // Updated from llama_load_model_from_file
    model.reset(llama_model_load_from_file(model_path.c_str(), m_params));

    if (!model) {
        std::cerr << "Error: Failed to load model." << std::endl;
        return false;
    }

    auto c_params = llama_context_default_params();
    c_params.n_ctx = n_ctx;
    c_params.n_batch = n_ctx;

    // Updated from llama_new_context_with_model
    ctx.reset(llama_init_from_model(model.get(), c_params));

    if (!ctx) {
        std::cerr << "Error: Failed to create context." << std::endl;
        return false;
    }

    return true;
}

std::vector<llama_token> LlamaClient::tokenize(const std::string& text, bool add_special) {
    // New API: Grab the vocab pointer from the model
    const struct llama_vocab* vocab = llama_model_get_vocab(model.get());
    
    int n_tokens = text.length() + (add_special ? 2 : 0);
    std::vector<llama_token> tokens(n_tokens);
    
    // Updated: passing 'vocab' as first argument
    n_tokens = llama_tokenize(vocab, text.c_str(), text.length(), tokens.data(), tokens.size(), add_special, true);
    
    if (n_tokens < 0) {
        tokens.resize(-n_tokens);
        llama_tokenize(vocab, text.c_str(), text.length(), tokens.data(), tokens.size(), add_special, true);
    } else {
        tokens.resize(n_tokens);
    }
    return tokens;
}

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