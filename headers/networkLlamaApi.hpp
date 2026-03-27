#include <iostream>
#include <string>
#include <curl/curl.h>
#include <functional>
#include <sstream>
#include <nlohmann/json.hpp>


using LogFn = std::function<void(const std::string&)>;
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}


std::string llama_completion(const std::string& prompt,
                             std::string curlOptUrl = "http://localhost:8080/completion",
                             std::string nPredict = "5",
                             LogFn logger = nullptr,
                            std::string authToken = "") {
    CURL* curl = curl_easy_init();
    std::string response;
    if (!curl) {
        if (logger) logger("curl_easy_init() failed");
        return "";
    }
    
    std::ostringstream ss;
    ss << R"({"prompt": ")" << prompt << R"(", "n_predict": )" << nPredict << R"(, "temperature": 0.7 })";
    if (authToken != ""){
        ss << R"({"prompt": ")" << prompt << R"(", "n_predict": )" << nPredict << R"(, "temperature": 0.7 , "Authorization": )"<< authToken << R"(})";
    }
    std::string json = ss.str();
    struct curl_slist* headers = NULL;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    logger(std::string("curlOpt URL: ") + curlOptUrl);
    curl_easy_setopt(curl, CURLOPT_URL, curlOptUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);

    if (res != CURLE_OK) {
        std::string err = std::string("Request failed: ") + curl_easy_strerror(res);
        if (logger) logger(err);
        else std::cerr << err << std::endl;
    }

    curl_easy_cleanup(curl);
    curl_slist_free_all(headers);

    return response;
}
std::string llama_completion_content(const std::string& prompt,
                                     std::string curlOptUrl = "http://localhost:8080/completion",
                                     std::string nPredict = "5",
                                     LogFn logger = nullptr) {
    std::string raw = llama_completion(prompt, curlOptUrl, nPredict, logger);

    if (raw.empty()) return "";

    try {
        auto j = nlohmann::json::parse(raw);

        // adjust depending on your API response format
        if (j.contains("content"))
            return j["content"].get<std::string>();

        // common alternative formats
        //if (j.contains("choices") && !j["choices"].empty()) {
        //    if (j["choices"][0].contains("text"))
        //        return j["choices"][0]["text"].get<std::string>();
        //    if (j["choices"][0].contains("message") &&
        //        j["choices"][0]["message"].contains("content"))
        //        return j["choices"][0]["message"]["content"].get<std::string>();
        //}

    } catch (const std::exception& e) {
        if (logger) logger(e.what());
    }

    return "";
}
std::string extract_content(const std::string& response) {
    auto j = nlohmann::json::parse(response);
    return j["content"].get<std::string>();
}