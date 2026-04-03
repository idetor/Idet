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


struct cacheAction {
    std::string action;
    std::string bufferDiffrence;
    int keyPressed;
    int cursorX;
    int cursorY;
};

void warnQuitWithUnsavedChanges() {
    while (true) {
        clear();
        mvprintw(0, 0, "You have unsaved changes. Press 'q' again to quit without saving, or any other key to cancel.");
        refresh();
        int ch = getch();
        if (ch == 'q' || ch == 'Q') {
            endwin();
            std::exit(0);
        } else {
            break; // cancel quit
        }
    }
}

int getUtf8StrLen(const std::string& str) {
    int len = 0;
    for (size_t i = 0; i < str.size();) {
        unsigned char c = static_cast<unsigned char>(str[i]);
        if ((c & 0x80) == 0) { // 1 byte
            i += 1;
        } else if ((c & 0xE0) == 0xC0) { // 2 bytes
            i += 2;
        } else if ((c & 0xF0) == 0xE0) { // 3 bytes
            i += 3;
        } else if ((c & 0xF8) == 0xF0) { // 4 bytes
            i += 4;
        } else {
            i += 1; 
        }
        len++;
    }
    return len;
}


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

std::string strVecToString(const std::vector<std::string>& vec) {
    std::string result;
    for (const auto& str : vec) {
        result += str + "\n";
    }
    if (!result.empty()) {
        result.pop_back(); // Remove the last newline
    }
    return result;
}

std::string jsonToString(const json& j) {
    return j.dump();
}   


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

// UTF-8 utility: Get the byte length of a UTF-8 character at position pos
inline int getUtf8CharLen(const std::string& str, size_t pos) {
    if (pos >= str.size()) return 0;
    unsigned char c = static_cast<unsigned char>(str[pos]);
    if ((c & 0x80) == 0) return 1;           // 0xxxxxxx (1 byte)
    if ((c & 0xE0) == 0xC0) return 2;        // 110xxxxx (2 bytes)
    if ((c & 0xF0) == 0xE0) return 3;        // 1110xxxx (3 bytes)
    if ((c & 0xF8) == 0xF0) return 4;        // 11110xxx (4 bytes)
    return 1;                                 // invalid, treat as 1 byte
}

// UTF-8 utility: Find the start position of the UTF-8 character containing byte at pos
inline size_t getUtf8CharStart(const std::string& str, size_t pos) {
    if (pos > str.size()) pos = str.size();
    while (pos > 0) {
        unsigned char c = static_cast<unsigned char>(str[pos - 1]);
        if ((c & 0xC0) != 0x80) break;  // Not a continuation byte, so pos-1 is the start
        --pos;
    }
    return pos;
}
