#include <ncurses.h>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <map>
#include <nlohmann/json.hpp>
#include <string_view>
#include <iostream>



// plain join (returns embedded newlines)
std::string joinVecLines(const std::vector<std::string>& arr) {
    std::string out;
    out.reserve(arr.size() * 16);
    for (size_t i = 0; i < arr.size(); ++i) {
        out += arr[i];
        if (i + 1 < arr.size()) out += '\n';
    }
    return out;
}

// JSON-escape a string so it can be embedded as a JSON string value
std::string escapeForJson(const std::string &s) {
    std::ostringstream o;
    for (unsigned char c : s) {
        switch (c) {
            case '\"': o << "\\\""; break;
            case '\\': o << "\\\\"; break;
            case '\b': o << "\\b";  break;
            case '\f': o << "\\f";  break;
            case '\n': o << "\\n";  break;
            case '\r': o << "\\r";  break;
            case '\t': o << "\\t";  break;
            default:
                if (c < 0x20) {
                    o << "\\u"
                      << std::hex << std::uppercase << std::setw(4) << std::setfill('0')
                      << static_cast<int>(c)
                      << std::dec << std::nouppercase;
                } else {
                    o << c;
                }
        }
    }
    return o.str();
}
std::string getStingFromVec(const std::vector<std::string>& inArray){
    // Return raw string without JSON escaping - let nlohmann::json handle it
    return joinVecLines(inArray);
}

using json = nlohmann::json;

class ConfigLoader {
private:
    json data;

    json sort_json(const json& input) {
        if (input.is_object()) {
            std::map<std::string, json> sorted_map;
            // Insert into map (automatically sorts keys)
            for (auto& [key, value] : input.items()) {
                sorted_map[key] = sort_json(value);
            }
            // Convert back to json object
            json result;
            for (auto& [key, value] : sorted_map) {
                result[key] = value;
            }

            return result;
        }
        else if (input.is_array()) {
            json result = json::array();
            for (const auto& item : input) {
                result.push_back(sort_json(item));
            }
            return result;
        }
        return input; // primitive types
    }
public:
    ConfigLoader(const std::string& filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            throw std::runtime_error("Could not open config file");
        }
        json raw;
        file >> raw;
        data = sort_json(raw);
    }
    json get() const {
        return data;
    }
    void print() const {
        std::cout << data.dump(4) << std::endl;
    }
};
