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
//#include "light/bash.hpp"


struct fileElements {
    int lastModified;
    bool isChanged;
    int selStartX;
    int selStartY;
    int selEndX;
    int selEndY;
    int cursorX;
    int cursorY;
};

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

struct tabOverlayParams {
    bool exists;
    int cursorX;
    int cursorY;
    int startOfWordX;
    int startOfWordY;
    std::vector<std::string> buffer;
    
    
    std::string cachedWord;
    std::string cachedCompareString;
    int cachedCursorX = -1;
    int cachedCursorY = -1;
    bool needsUpdate = true;
};

std::string fileElementsElementToString(fileElements FileElement) {
    std::string returnMessage;
    returnMessage.append("lastModified : " + std::to_string(FileElement.lastModified) + "\n");
    returnMessage.append("isChanged : " + std::string(FileElement.isChanged ? "true" : "false") + "\n");
    returnMessage.append("selStartX : " + std::to_string(FileElement.selStartX) + "\n");
    returnMessage.append("selStartY : " + std::to_string(FileElement.selStartY) + "\n");
    returnMessage.append("selEndX : " + std::to_string(FileElement.selEndX) + "\n");
    returnMessage.append("selEndY : " + std::to_string(FileElement.selEndY) + "\n");
    returnMessage.append("cursorX : " + std::to_string(FileElement.cursorX) + "\n");
    returnMessage.append("cursorY : " + std::to_string(FileElement.cursorY) + "\n");
    return returnMessage;
}


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
    bool afterAlso;
};
struct closeXPos{
    bool hasPos;
    int xPos;
    bool hasSecondPos;
    int secondXPos;
};

std::string beforeCursor(std::string lineContent, int cursorX) {
    if (cursorX <= 0) {
        return "";
    }
    if (cursorX > lineContent.length()) {
        cursorX = lineContent.length();
    }
    int startPos = cursorX - 1;
    while (startPos >= 0 && lineContent[startPos] != ' ') {
        startPos--;
    }
    startPos++;  
    int length = cursorX - startPos;
    return lineContent.substr(startPos, length);
}


std::string toSpace(std::string lineString){
    std::string constructBackString = "";
    for (int i = 0; i < lineString.size(); i++){
        if (lineString[i] == ' '){
            return constructBackString;
        }
        else {
            constructBackString = constructBackString + lineString[i];
        }
    }
    return constructBackString;
}

std::string getWordFromCords(std::string lineContent , posCords cords){
    
    if (cords.x < 0 || cords.x > lineContent.length()) {
        return "";
    }
    std::string fromPosString = lineContent.substr(cords.x);
    return toSpace(fromPosString);
}




closeXPos getClosestPosCordsX(std::string lineString, std::string compareString, int ignoreNFirst = 0, int getNPos = 0){
    // can also check for multiple position by ignoring first n found positions and returning the next one - for now only return first position
    // checks if compareString is in lineString

    if (lineString.find(compareString) != std::string::npos){
        if (ignoreNFirst > 0){
            size_t pos = lineString.find(compareString);
            int foundCount = 0;
            while (pos != std::string::npos && foundCount < ignoreNFirst) {
                foundCount++;
                pos = lineString.find(compareString, pos + compareString.length());
            }
            if (pos != std::string::npos){
                return {true, (int)pos, false, -1};
            }
            else{
                return {false, -1, false, -1};
            }
        }
        return {true, (int)lineString.find(compareString)};
    }
    return {false, -1};
}
posCords findInBuffer(std::vector<std::string> &buffer, std::string compareString, int ignoreNFirst = 0, int getNPos = 0){
    for (int y = ignoreNFirst; y < buffer.size(); y++){
        closeXPos xPos = getClosestPosCordsX(buffer[y], compareString, ignoreNFirst, getNPos);
        if (xPos.hasPos){
            return {true, xPos.xPos, y};
        }
    }
    return {false, -1 , -1};
}

// Find the Nth occurrence of a string starting from a given position
posCords findNextInBuffer(std::vector<std::string> &buffer, std::string compareString, int startY, int startX) {
    // Start searching from startY and startX
    for (int y = startY; y < buffer.size(); y++) {
        int searchStartX = (y == startY) ? startX : 0;
        size_t pos = buffer[y].find(compareString, searchStartX);
        
        if (pos != std::string::npos) {
            return {true, (int)pos, y};
        }
    }
    
    // If not found going forward, wrap around to beginning
    for (int y = 0; y < startY; y++) {
        size_t pos = buffer[y].find(compareString);
        if (pos != std::string::npos) {
            return {true, (int)pos, y};
        }
    }
    
    return {false, -1, -1};
}

int waitOnKeyPress(){
    while (true) {
        int ch = getch();
        if (ch != ERR) {
            return ch;
        }
    }
}


// Find the last occurrence of a string in buffer
posCords findLastInBuffer(std::vector<std::string> &buffer, std::string compareString) {
    for (int y = buffer.size() - 1; y >= 0; y--) {
        size_t pos = buffer[y].rfind(compareString);
        if (pos != std::string::npos) {
            return {true, (int)pos, y};
        }
    }
    return {false, -1, -1};
}
struct colorPair {
    int pairNum;
    int fgColor;
    int bgColor;
};
int waitForKeyPress(int key1, int key2) {
    int ch;
    while (true) {
        ch = getch();
        if (ch == key1 || ch == key2) {
            return ch;
        }
    }
}
std::string posCordsToString(posCords cords){
    if (!cords.exists) return "Not found";
    return "X: " + std::to_string(cords.x) + " Y: " + std::to_string(cords.y);
}
std::string posCordsVecToString(std::vector<posCords> cordsVec){
    std::string result = "";
    for (const auto& cords : cordsVec) {
        result += posCordsToString(cords) + "\n";
    }
    return result.empty() ? "No positions found" : result;
}
void fillInVecPosCords(std::vector<posCords> &vec, std::vector<std::string> &buffer, std::string searchString){
    for (int y = 0; y < buffer.size(); y++){
        size_t pos = buffer[y].find(searchString);
        while (pos != std::string::npos){
            vec.push_back({true, (int)pos, y});
            pos = buffer[y].find(searchString, pos + searchString.size());
        }
    }
    // remove first item
    if (!vec.empty()){
        vec.erase(vec.begin());
    }
}

void searchOverlay(std::vector<std::string>& buffer, int& cursorX, int& cursorY, bool& searchActive , std::string& searchTerm,
int& lastFoundX, int& lastFoundY, std::vector<posCords>& searchResults) {
    searchActive = true;
    int searchRow = LINES - 2;
    bool firstAction = true;
    std::string searchSuggestion = "";
    


    while (true) {
        move(searchRow, 0);
        clrtoeol();
        // make suggestion gray
        attron(COLOR_PAIR(100));
        mvprintw(searchRow, 0, "Search: %s", searchTerm.c_str());
        attroff(COLOR_PAIR(100));
        attron(COLOR_PAIR(110));
        mvprintw(searchRow, 8 + searchTerm.size(), "%s", searchSuggestion.c_str());
        attroff(COLOR_PAIR(110));
        refresh();
        int ch = getch();
        
        if (ch == 27) {
            searchActive = false;
            break; // ESC key 
        } 
        //also handle backspace
        else if (ch == KEY_BACKSPACE || ch == 263 ) {
            if (!firstAction){
                if (!searchTerm.empty()) {
                    searchTerm.pop_back();
                    searchSuggestion = "";
                    lastFoundY = -1;
                    lastFoundX = -1;
                }
            }else{
                searchTerm = "";
                continue;
            }

        } else if (ch == '\n' || ch == '\r' || ch == 10 || ch == 13 || ch == KEY_ENTER) {
            
            if (!searchTerm.empty()) {
                posCords cords;
                if (lastFoundY >= 0 && lastFoundX >= 0) {
                    
                    cords = findNextInBuffer(buffer, searchTerm, lastFoundY, lastFoundX + (int)searchTerm.length());
                } else {
                    
                    cords = findInBuffer(buffer, searchTerm);
                }
                
                if (cords.exists) {
                    cursorX = cords.x;
                    cursorY = cords.y;
                    lastFoundY = cords.y;
                    lastFoundX = cords.x;
                }
                fillInVecPosCords(searchResults, buffer, searchTerm);
                break;
            }
            break;
        } else if (ch == KEY_END) {
            
            if (!searchTerm.empty()) {
                posCords cords = findLastInBuffer(buffer, searchTerm);
                if (cords.exists) {
                    cursorX = cords.x;
                    cursorY = cords.y;
                    lastFoundY = cords.y;
                    lastFoundX = cords.x;
                }
            }
        } else if (ch == KEY_HOME) {
            
            if (!searchTerm.empty()) {
                posCords cords = findInBuffer(buffer, searchTerm);
                if (cords.exists) {
                    cursorX = cords.x;
                    cursorY = cords.y;
                    lastFoundY = cords.y;
                    lastFoundX = cords.x;
                }
            }
        } else if (ch == KEY_NPAGE) {
            
            if (!searchTerm.empty() && lastFoundY >= 0) {
                int nextSearchY = lastFoundY + (LINES / 2);
                posCords cords = findNextInBuffer(buffer, searchTerm, nextSearchY, 0);
                if (cords.exists) {
                    cursorX = cords.x;
                    cursorY = cords.y;
                    lastFoundY = cords.y;
                    lastFoundX = cords.x;
                }
            }
        } else if (isprint(ch)) {
            searchTerm += static_cast<char>(ch);
            lastFoundY = -1;
            lastFoundX = -1;
        }
        //also utf-8 characters
        else if (ch >= 128 && ch <= 255) {
            lastFoundY = -1;
            lastFoundX = -1;
            std::string utf8_char;
            utf8_char += static_cast<char>(ch);
            int remaining_bytes = 0;
            unsigned char uc = static_cast<unsigned char>(ch);
            if ((uc & 0xE0) == 0xC0) remaining_bytes = 1;
            else if ((uc & 0xF0) == 0xE0) remaining_bytes = 2;
            else if ((uc & 0xF8) == 0xF0) remaining_bytes = 3;
            for (int i = 0; i < remaining_bytes; ++i) {
                int next_byte = getch();
                if (next_byte > 0) {
                    utf8_char += static_cast<char>(next_byte);
                }
            }
            searchTerm += utf8_char;
        }
        // TAB - accept suggestion
        else if (ch == '\t') {
            if (!searchSuggestion.empty()) {
                searchTerm += searchSuggestion;
                searchSuggestion = "";
                lastFoundY = -1;
                lastFoundX = -1;
            }
        }
        // get suggestion
        if (!searchTerm.empty()) {
            posCords cords = findInBuffer(buffer, searchTerm);
            if (cords.exists) {
                searchSuggestion = buffer[cords.y].substr(cords.x + searchTerm.size());
            } else {
                searchSuggestion = "";
            }
        } else {
            searchSuggestion = "";
        }
        if (ch > 0){
            firstAction = false;
        }
    }
}

posCords suggestSearch(std::string startString, std::vector<std::string> &buffer){
    posCords exampleCords = findInBuffer(buffer, startString);
    if (exampleCords.exists){
        return exampleCords;
    }
    return {false, -1, -1};
}

void clearLine(int lineNum){
    //clears a line without moving the cursor
    move(lineNum, 0);
    clrtoeol();
}

void emptySearchOverlay(std::string searchTerm){
    // get current cursor position
    int funccursorX, funccursorY;
    getyx(stdscr, funccursorY, funccursorX);
    int searchRow = LINES - 2;
    
    clearLine(searchRow);
    move(searchRow, 0);
    attron(COLOR_PAIR(100));
    
    mvprintw(searchRow, 0, "Search: %s", searchTerm.c_str());
    attroff(COLOR_PAIR(100));
    refresh();
    move(funccursorY, funccursorX);
}

std::string getWordFromCords(int cordX,int cordY, std::vector<std::string> &buffer){
    if (cordY < 0 || cordY >= buffer.size()) return "";
    const std::string& line = buffer[cordY];
    if (cordX < 0 || cordX > line.size()) return "";
    
    // Find word boundaries
    int start = cordX;
    while (start > 0 && line[start - 1] != ' ') {
        start--;
    }
    
    int end = cordX;
    while (end < line.size() && line[end] != ' ') {
        end++;
    }
    
    return line.substr(start, end - start);
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

std::string utf8substr(std::string string, int start, int end) {
    int byteStart = 0;
    int byteEnd = 0;
    int charCount = 0;
    for (int i = 0; i < string.length(); i++) {
        if (charCount == start) {
            byteStart = i;
        }
        if (charCount == end) {
            byteEnd = i;
            break;
        }
        if ((string[i] & 0xC0) != 0x80) {
            charCount++;
        }
    }
    if (charCount == end || byteEnd == 0) {
        byteEnd = string.length();
    }
    
    return string.substr(byteStart, byteEnd - byteStart);
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

bool NdirectspacesBefore(std::string line, int cursorX, int numSpaces) {
    
    if (cursorX < numSpaces) {
        return false;
    }
    
    for (int i = cursorX - numSpaces; i < cursorX; i++) {
        if (line[i] != ' ') {
            return false;
        }
    }
    return true;
}
int NdirectspacesBeforeNum(const std::string& line, int cursorX) {
    
    int bytePos = 0;
    int charCount = 0;
    for (size_t i = 0; i < line.length() && charCount < cursorX; i++) {
        unsigned char c = static_cast<unsigned char>(line[i]);
        if ((c & 0xC0) != 0x80) {  
            charCount++;
        }
        bytePos++;
    }
    
    int spaceCount = 0;
    for (int i = bytePos - 1; i >= 0 && line[i] == ' '; i--) {
        spaceCount++;
    }
    
    return spaceCount;
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

bool stringContainsString(std::string string , std::string containString){
    // checks if a string is in another string
    return string.find(containString) != std::string::npos;
}


// NOTE: colordraw is deprecated - use draw() function in main.cpp instead which now includes syntax highlighting


std::string tolowerString(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}
bool isDirectory(std::string filename) {
    struct stat buffer;
    if (stat(filename.c_str(), &buffer) != 0) {
        return false; 
    }
    return S_ISDIR(buffer.st_mode); 
}