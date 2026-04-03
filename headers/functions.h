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
    int affectedStartLine;           
    std::vector<std::string> removedLines;
    std::vector<std::string> insertedLines;
    int keyPressed;
    int cursorX;
    int cursorY;
    int pasteSize;  
};


inline cacheAction createDiff(
    const std::vector<std::string>& oldBuffer,
    const std::vector<std::string>& newBuffer,
    int cursorX, int cursorY, int keyPressed, int pasteSize = 0) {
    
    cacheAction diff;
    diff.action = "edit";
    diff.keyPressed = keyPressed;
    diff.cursorX = cursorX;
    diff.cursorY = cursorY;
    diff.pasteSize = pasteSize;
    diff.affectedStartLine = 0;
    
    if (oldBuffer == newBuffer) {
        diff.affectedStartLine = 0;
        return diff;
    }
    
    size_t firstDiff = 0;
    size_t commonStart = 0;
    while (commonStart < oldBuffer.size() && commonStart < newBuffer.size() &&
           oldBuffer[commonStart] == newBuffer[commonStart]) {
        commonStart++;
    }
    
    if (commonStart <= oldBuffer.size()) {
        diff.affectedStartLine = commonStart;
        for (size_t i = commonStart; i < oldBuffer.size(); ++i) {
            diff.removedLines.push_back(oldBuffer[i]);
        }
    }
    
    if (commonStart <= newBuffer.size()) {
        for (size_t i = commonStart; i < newBuffer.size(); ++i) {
            diff.insertedLines.push_back(newBuffer[i]);
        }
    }
    return diff;
}


inline void applyDiff(std::vector<std::string>& buffer, const cacheAction& diff) {
    int lineNum = diff.affectedStartLine;
    if (lineNum < buffer.size()) {
        buffer.erase(buffer.begin() + lineNum, buffer.end());
    }
    for (const auto& line : diff.insertedLines) {
        buffer.push_back(line);
    }
    if (buffer.empty()) {
        buffer.push_back("");
    }
}

struct posCords {
    bool exists;
    int x;
    int y;
};
struct closeXPos{
    bool hasPos;
    int xPos;
};


closeXPos getClosestPosCordsX(std::string lineString, std::string compareString){
    // checks if compareString is in lineString
    if (lineString.find(compareString) != std::string::npos){
        return {true, (int)lineString.find(compareString)};
    }
    return {false, -1};
}
posCords findInBuffer(std::vector<std::string> &buffer, std::string compareString){
    for (int y = 0; y < buffer.size(); y++){
        closeXPos xPos = getClosestPosCordsX(buffer[y], compareString);
        if (xPos.hasPos){
            return {true, xPos.xPos, y};
        }
    }
    return {false, -1 , -1};
}



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

void switchStartEnd(int& selStartX, int& selEndX) {
    std::swap(selStartX, selEndX);
}


int getUtf8CharLenReverse(std::string string_before){
    if (string_before.empty()) return 0;
    unsigned char c = static_cast<unsigned char>(string_before.back());
    if ((c & 0x80) == 0) return 1;           // 0xxxxxxx (1 byte)
    if ((c & 0xE0) == 0xC0) return 2;        // 110xxxxx (2 bytes)
    if ((c & 0xF0) == 0xE0) return 3;        // 1110xxxx (3 bytes)
    if ((c & 0xF8) == 0xF0) return 4;        // 11110xxx (4 bytes)
    return 1;                                 // invalid, treat as 1 byte
}


std::string getWordSelectionRight(const std::string rightString) {
    std::string wordRight = "";

    for (char c : rightString) {
        if (c == ' ') {
            if (c == rightString[0]) {
                continue; // skip leading spaces
            }
            return wordRight; 
        }
        wordRight += c;
    }

    return wordRight; 
}

std::string subtractStringLeft(const std::string fullString, int subtraction) {
    if (subtraction <= 0) {
        return fullString; 
    }

    if (subtraction >= fullString.length()) {
        return ""; 
    }

    return fullString.substr(subtraction);
}


std::string getWordSelectionLeft(const std::string& lineContent) {
    if (lineContent.empty()) return "";
    
    std::string wordLeft;
    bool foundWord = false;
    
    
    for (auto it = lineContent.rbegin(); it != lineContent.rend(); ++it) {
        if (*it == ' ') {
            
            if (foundWord) {
                break;
            }
            
            continue;
        }
        
        foundWord = true;
        wordLeft = *it + wordLeft; 
    }
    return wordLeft;
}
std::string subtractStringRight(const std::string& lineContent, int cursorXPos) {
    if (cursorXPos < 0 || cursorXPos > (int)lineContent.size()) {
        return lineContent; // invalid position, return original
    }
    
    return lineContent.substr(0, cursorXPos);
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
