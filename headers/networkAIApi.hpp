#include <iostream>
#include <string>
#include <curl/curl.h>
#include <functional>
#include <sstream>
#include <nlohmann/json.hpp>
#include <regex>
using LogFn = std::function<void(const std::string&)>;
// ------------------------------------------------------------------
// libcurl helpers
static size_t WriteCallback(void* contents, size_t size, size_t nmemb,
                            void* userp) {
    static_cast<std::string*>(userp)->append(static_cast<char*>(contents),
                                             size * nmemb);
    return size * nmemb;
}

// ------------------------------------------------------------------
// LlamaCPP helpers (unchanged)
// ------------------------------------------------------------------
std::string llama_completion(const std::string& prompt,
                             const std::string& url = "http://localhost:8080/completion",
                             const std::string& nPredict = "5",
                             LogFn logger = nullptr,
                             const std::string& authToken = "") {
    CURL* curl = curl_easy_init();
    if (!curl) {
        if (logger) logger("curl_easy_init() failed");
        return "";
    }

    nlohmann::json payload;
    payload["prompt"]      = prompt;
    payload["n_predict"]   = nPredict;
    payload["temperature"] = 0.7;
    if (!authToken.empty())
        payload["Authorization"] = authToken;  // just a placeholder

    std::string json = payload.dump();

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    if (logger) logger("curl URL: " + url);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    std::string response;
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std::string err = "Request failed: " + std::string(curl_easy_strerror(res));
        if (logger) logger(err);
        else std::cerr << err << std::endl;
    }

    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
    return response;
}

std::string llama_completion_content(const std::string& prompt,
                                     const std::string& url = "http://localhost:8080/v1/completion",
                                     const std::string& nPredict = "5",
                                     LogFn logger = nullptr) {
    const std::string raw = llama_completion(prompt, url, nPredict, logger);
    if (raw.empty()) return "";

    // Guard against non‑JSON responses (e.g., HTTP error messages)
    if (!raw.empty() && raw[0] != '{' && raw[0] != '[') {
        if (logger) logger("Non‑JSON response received: " + raw.substr(0, 100));
        return raw; // return raw string for caller to handle
    }
    try {
        auto j = nlohmann::json::parse(raw);
        if (j.contains("content"))
            return j["content"].get<std::string>();
    } catch (const std::exception& e) {
        if (logger) logger(e.what());
    }
    return "";
}

// ------------------------------------------------------------------
// Ollama helpers (new)
// ------------------------------------------------------------------

std::string ollama_completion(const std::string& prompt,
                              const std::string& url = "http://localhost:11434/api/generate",
                              const std::string& nPredict = "5",
                              LogFn logger = nullptr,
                              const std::string& model = "gpt-oss:20b") {
    CURL* curl = curl_easy_init();
    if (!curl) {
        if (logger) logger("curl_easy_init() failed");
        return "";
    }

    nlohmann::json payload;
    payload["model"]  = model;
    payload["prompt"] = prompt;
    payload["stream"] = false;
    payload["think"] = false;
    // Put generation params inside "options"
    nlohmann::json options;
    options["num_predict"] = std::stoi(nPredict);
    options["temperature"] = 0.0;

    payload["options"] = options;

    std::string json = payload.dump();

    if (logger) logger("Ollama payload: " + json);

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");

    if (logger) logger("curl URL: " + url);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);

    std::string response;
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        std::string err = "Request failed: " + std::string(curl_easy_strerror(res));
        if (logger) logger(err);
        else std::cerr << err << std::endl;
    }

    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);
    return response;
}

std::string ollama_completion_content(const std::string& prompt,
                                      const std::string& url = "http://localhost:11434/api/generate",
                                      const std::string& nPredict = "5",
                                      LogFn logger = nullptr,
                                      const std::string& model = "llama3") {
    const std::string raw = ollama_completion(prompt, url, nPredict, logger, model);
    if (raw.empty()) return "";

    // Guard against non‑JSON responses
    if (!raw.empty() && raw[0] != '{' && raw[0] != '[') {
        if (logger) logger("Non‑JSON response received: " + raw.substr(0, 100));
        return raw;
    }
    try {
        auto j = nlohmann::json::parse(raw);
        logger("Recived ollama output:" + raw);
        /* Ollama’s `/api/generate` returns a very simple structure:
         *   { "response": "...", "details": {...} }
         * so we just pull the “response” field.
         */
        if (j.contains("response"))
            return j["response"].get<std::string>();
    } catch (const std::exception& e) {
        if (logger) logger(e.what());
    }
    return "";
}

static std::string toLowerCopy(const std::string &s) {
    std::string t = s;
    std::transform(t.begin(), t.end(), t.begin(), [](unsigned char c){ return std::tolower(c); });
    return t;
}

std::string getIpAddressOfString(const std::string& IpString, LogFn logger = nullptr) {
    auto log = [&](const std::string &msg){
        if (logger) logger(msg);
    };

    // 1) Try IPv4
    static const std::regex ipRegex(
        R"((?:(?:25[0-5]|2[0-4]\d|1\d{2}|[1-9]?\d)\.){3}(?:25[0-5]|2[0-4]\d|1\d{2}|[1-9]?\d))"
    );
    std::smatch match;
    if (std::regex_search(IpString, match, ipRegex)) {
        log("getIpAddressOfString: Ip-Address found: " + match.str(0));
        return match.str(0);
    }
    static const std::regex ipv6LoopbackRegex(R"((?:\[(::1)\]|(::1))(?:[:\s]|$))");
    if (std::regex_search(IpString, match, ipv6LoopbackRegex)) {
        log("getIpAddressOfString: IPv6 loopback found, returning 127.0.0.1");
        return "127.0.0.1";
    }

    std::string lower = toLowerCopy(IpString);

    static const std::regex localhostRegex(R"((?:^|[^a-z0-9\-])localhost(?:[:/\s]|$))", std::regex_constants::icase);
    if (std::regex_search(lower, match, localhostRegex)) {
        log("getIpAddressOfString: 'localhost' found, returning 127.0.0.1");
        return "127.0.0.1";
    }

    log("getIpAddressOfString: no Ip-Address found");
    return std::string();
}


std::string AiCompletion(const std::string& prompt,
                         std::string curlOptUrl = "http://localhost:8080/v1/completion",
                         std::string nPredict     = "5",
                         LogFn logger = nullptr,
                         std::string AiHost = "llamacpp", std::string ollamaModel = "") {
    if (!logger) logger = [](const std::string&) {};  // no-op if null
    logger("Using AiProvider: " + AiHost);
    
    if (AiHost == "llamacpp") {
        std::string llamaUrl = curlOptUrl;
        // Only append /completion if endpoint is not already present
        if (llamaUrl.find("/completion") == std::string::npos && 
            llamaUrl.find("/v1/completion") == std::string::npos) {
            llamaUrl += "/completion";
        }
        return llama_completion_content(prompt, llamaUrl, nPredict, logger);
    } else if (AiHost == "ollama") {
        logger("inside Ollama call");
        if (ollamaModel == ""){
            logger("Ollama: No model given");
            return "";
        }else{
            
            std::string ipAddress = getIpAddressOfString(curlOptUrl, logger);
            curlOptUrl = ("http://" + ipAddress + ":11434/api/generate");
            logger("Constructed new CurlOptUrl: " + curlOptUrl);
            logger("nPredict: " + nPredict);
            ollama_completion_content(prompt,curlOptUrl,nPredict,logger,ollamaModel);
        }

    } else {
        if (logger) logger("Unsupported AI provider: " + AiHost);
        return "";
    }
    return "";
}

