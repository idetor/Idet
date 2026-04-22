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
#include <cstdlib>

//#include "light/bash.hpp"

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
class cursorElement {
    public:
        int X;
        int Y;
        cursorElement()
            : X(0),
              Y(0) {}
};

class SelectionElements {
    public:
        bool active;
        int startX;
        int startY;
        int endX;
        int endY;
        
        // legacy
        int selStartX;
        int selStartY;
        int selEndX;
        int selEndY;
        bool selectionActive;
    SelectionElements()
        : 
          active(false),
          startX(0),
          startY(0),
          endX(0),
          endY(0),
          //legacy
          selStartX(0),
          selStartY(0),
          selEndX(0),
          selEndY(0),
          selectionActive(false) {}
};

class SearchElement {
    public:
        bool active; //
        std::string term; //
        int lastX; //
        int lastY; //
        int count; //
        std::vector<posCords> results; 
        // legacy
        bool activeSearch;
        std::string searchTerm;
        int SearchLastFoundX;
        int SearchLastFoundY;
        int searchcount;
        std::vector<posCords> searchResults;

    SearchElement()
        : active(false),
          term(""),
          lastX(-1),
          lastY(-1),
          count(0),
          //legacy
          activeSearch(false),
          searchTerm(""),
          SearchLastFoundX(-1),
          SearchLastFoundY(-1),
          searchcount(0) {}
};

class AiProps{
    public:
        std::string AiProvider;
        std::string authToken;
        std::string llamaCompletionHost;
        std::string llamaCompletionNPredict;
        std::string ollamaModel;
        std::string modelPath;
        int maxInlinePromptSize;
        int inlineSuggestionNPredict;
        int AUTO_SUGGESTION_DELAY;
        
        

    AiProps() 
        : AiProvider("llamacpp"),
          authToken(""),
          llamaCompletionHost("http://localhost:8080"),
          llamaCompletionNPredict("5"),
          ollamaModel("gpt-oss:20b"),
          inlineSuggestionNPredict(5),
          modelPath("/var/models/tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf"),
          maxInlinePromptSize(10000),
          AUTO_SUGGESTION_DELAY(3) {}
};

class configElement{
    public:
        int lineNumberScheme;
        int contentScheme;
        const size_t DEBUG_MAX;
        int maxCacheNum;
        bool multiFileMode;
        int tabSpaces;
    configElement()
        : lineNumberScheme(1),
        contentScheme(3),
        DEBUG_MAX(10000),
        maxCacheNum(100),
        multiFileMode(false),
        tabSpaces(4){}
};

class AiUtils{
    public:
        bool llamaInit;
        bool modelLoaded;
        bool showInlineSuggestion;
        bool inlineSuggestionExists;
        bool allowInlineSuggestion;
        bool autoSuggestionTriggered;
        AiUtils()
            : llamaInit(false),
              modelLoaded(false),
              showInlineSuggestion(false),
              inlineSuggestionExists(false),
              allowInlineSuggestion(false),
              autoSuggestionTriggered(false){}
};

struct FileProperties{
    int lastModifiedTime;
    bool unsavedChanges;
    int savedCacheIndex;

};

struct fileElements {
    int lastModified;
    bool isChanged;
    cursorElement cursor;

    SelectionElements selection;
    // legacy
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

    int startOfWordX;
    int startOfWordY;
    std::vector<std::string> buffer;
    cursorElement cursor;

    std::string cachedWord;
    std::string cachedCompareString;
    int cachedCursorX = -1;
    int cachedCursorY = -1;
    bool needsUpdate = true;
    // legacy
    int cursorX;
    int cursorY;
};

std::string expandPath(const std::string& path) {

    if (!path.empty() && path[0] == '~') {
        const char* home = std::getenv("HOME");
        if (home) {
            return std::string(home) + path.substr(1);
        }
    }
    return path;
}

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
    
    size_t commonStart = 0;
    while (commonStart < oldBuffer.size() && commonStart < newBuffer.size() &&
           oldBuffer[commonStart] == newBuffer[commonStart]) {
        commonStart++;
    }
    
    
    diff.affectedStartLine = commonStart;
    
    for (size_t i = commonStart; i < oldBuffer.size(); ++i) {
        diff.removedLines.push_back(oldBuffer[i]);
    }
    
    for (size_t i = commonStart; i < newBuffer.size(); ++i) {
        diff.insertedLines.push_back(newBuffer[i]);
    }
    
    return diff;
}

inline void applyDiff(std::vector<std::string>& buffer, const cacheAction& diff) {
    int lineNum = diff.affectedStartLine;
    while ((int)buffer.size() < lineNum) {
        buffer.push_back("");
    }
    if (lineNum < (int)buffer.size()) {
        buffer.erase(buffer.begin() + lineNum, buffer.end());
    }
    for (const auto& line : diff.insertedLines) {
        buffer.push_back(line);
    }
    if (buffer.empty()) {
        buffer.push_back("");
    }
}


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

bool endswith(std::string string, std::string endString) {
    if (endString.length() > string.length()) {
        return false;
    }
    return string.compare(string.length() - endString.length(), endString.length(), endString) == 0;
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

void searchOverlay(std::vector<std::string>& buffer, int& cursorX, int& cursorY, SearchElement& search) {
    search.active = true;
    int searchRow = LINES - 2;
    bool firstAction = true;
    std::string searchSuggestion = "";
    


    while (true) {
        move(searchRow, 0);
        clrtoeol();
        // make suggestion gray
        attron(COLOR_PAIR(100));
        mvprintw(searchRow, 0, "Search: %s", search.term.c_str());
        attroff(COLOR_PAIR(100));
        attron(COLOR_PAIR(110));
        mvprintw(searchRow, 8 + search.term.size(), "%s", searchSuggestion.c_str());
        attroff(COLOR_PAIR(110));
        refresh();
        int ch = getch();
        
        if (ch == 27) {
            search.active = false;
            break; // ESC key 
        } 
        //also handle backspace
        else if (ch == KEY_BACKSPACE || ch == 263 ) {
            if (!firstAction){
                if (!search.term.empty()) {
                    search.term.pop_back();
                    searchSuggestion = "";
                    search.lastX = -1;
                    search.lastY = -1;
                }
            }else{
                search.term = "";
                continue;
            }

        } else if (ch == '\n' || ch == '\r' || ch == 10 || ch == 13 || ch == KEY_ENTER) {
            
            if (!search.term.empty()) {
                posCords cords;
                if (search.lastY >= 0 && search.lastX >= 0) {
                    
                    cords = findNextInBuffer(buffer, search.term, search.lastY, search.lastX + (int)search.term.length());
                } else {
                    
                    cords = findInBuffer(buffer, search.term);
                }
                
                if (cords.exists) {
                    cursorX = cords.x;
                    cursorY = cords.y;
                    search.lastY = cords.y;
                    search.lastX = cords.x;
                }
                fillInVecPosCords(search.results, buffer, search.term);
                break;
            }
            break;
        } else if (ch == KEY_END) {
            
            if (!search.term.empty()) {
                posCords cords = findLastInBuffer(buffer, search.term);
                if (cords.exists) {
                    cursorX = cords.x;
                    cursorY = cords.y;
                    search.lastY = cords.y;
                    search.lastX = cords.x;
                }
            }
        } else if (ch == KEY_HOME) {
            
            if (!search.term.empty()) {
                posCords cords = findInBuffer(buffer, search.term);
                if (cords.exists) {
                    cursorX = cords.x;
                    cursorY = cords.y;
                    search.lastY = cords.y;
                    search.lastX = cords.x;
                }
            }
        } else if (ch == KEY_NPAGE) {
            
            if (!search.term.empty() && search.lastY >= 0) {
                int nextSearchY = search.lastY + (LINES / 2);
                posCords cords = findNextInBuffer(buffer, search.term, nextSearchY, 0);
                if (cords.exists) {
                    cursorX = cords.x;
                    cursorY = cords.y;
                    search.lastY = cords.y;
                    search.lastX = cords.x;
                }
            }
        } else if (isprint(ch)) {
            search.term += static_cast<char>(ch);
            search.lastY = -1;
            search.lastX = -1;
        }
        //also utf-8 characters
        else if (ch >= 128 && ch <= 255) {
            search.lastY = -1;
            search.lastX = -1;
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
            search.term += utf8_char;
        }
        // TAB - accept suggestion
        else if (ch == '\t') {
            if (!searchSuggestion.empty()) {
                search.term += searchSuggestion;
                searchSuggestion = "";
                search.lastY = -1;
                search.lastX = -1;
            }
        }
        // get suggestion
        if (!search.term.empty()) {
            posCords cords = findInBuffer(buffer, search.term);
            if (cords.exists) {
                searchSuggestion = buffer[cords.y].substr(cords.x + search.term.size());
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

std::string joinVecLines(const std::vector<std::string>& arr) {
    std::string out;
    out.reserve(arr.size() * 16);
    for (size_t i = 0; i < arr.size(); ++i) {
        out += arr[i];
        if (i + 1 < arr.size()) out += '\n';
    }
    return out;
}

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

    std::string trim(const std::string& str) {
        size_t start = str.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        size_t end = str.find_last_not_of(" \t\r\n");
        return str.substr(start, end - start + 1);
    }

    std::string removeComment(const std::string& line) {
        size_t commentPos = line.find('#');
        if (commentPos != std::string::npos) {
            return trim(line.substr(0, commentPos));
        }
        return trim(line);
    }

    std::pair<std::string, std::string> parseLine(const std::string& line) {
        std::string cleaned = removeComment(line);
        
        if (cleaned.empty()) {
            return {"", ""};
        }

        size_t delimPos = cleaned.find('=');
        if (delimPos == std::string::npos) {
            throw std::runtime_error("Invalid config format: missing '=' in line: " + line);
        }

        std::string key = trim(cleaned.substr(0, delimPos));
        std::string value = trim(cleaned.substr(delimPos + 1));

        if (key.empty()) {
            throw std::runtime_error("Invalid config format: empty key in line: " + line);
        }

        return {key, value};
    }

    json parseValue(const std::string& value) {
        if (value == "true") return true;
        if (value == "false") return false;
        if (value == "null") return json();

        try {
            size_t idx;
            long long intVal = std::stoll(value, &idx);
            if (idx == value.length()) {
                return intVal;
            }
        } catch (...) {}


        try {
            size_t idx;
            double floatVal = std::stod(value, &idx);
            if (idx == value.length()) {
                return floatVal;
            }
        } catch (...) {}

        return value;
    }



    json sortJson(const json& input) {
        if (input.is_object()) {
            std::map<std::string, json> sortedMap;
            for (auto& [key, value] : input.items()) {
                sortedMap[key] = sortJson(value);
            }
            json result;
            for (auto& [key, value] : sortedMap) {
                result[key] = value;
            }
            return result;
        }
        else if (input.is_array()) {
            json result = json::array();
            for (const auto& item : input) {
                result.push_back(sortJson(item));
            }
            return result;
        }
        return input;
    }

public:

    ConfigLoader(const std::string& filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            throw std::runtime_error("Could not open config file: " + filepath);
        }

        json tempData;
        std::string line;
        int lineNumber = 0;

        while (std::getline(file, line)) {
            lineNumber++;
            std::string cleaned = removeComment(line);
            

            if (cleaned.empty()) {
                continue;
            }

            try {
                auto [key, value] = parseLine(line);
                if (!key.empty()) {
                    tempData[key] = parseValue(value);
                }
            } catch (const std::exception& e) {
                throw std::runtime_error(
                    std::string(e.what()) + " (line " + std::to_string(lineNumber) + ")"
                );
            }
        }

        file.close();
        data = sortJson(tempData);
    }


    json get() const {
        return data;
    }


    void print() const {
        std::cout << data.dump(4) << std::endl;
    }


    json get(const std::string& key) const {
        if (data.contains(key)) {
            return data[key];
        }
        return json();
    }


    bool has(const std::string& key) const {
        return data.contains(key);
    }
};

inline int getUtf8CharLen(const std::string& str, size_t pos) {
    if (pos >= str.size()) return 0;
    unsigned char c = static_cast<unsigned char>(str[pos]);
    if ((c & 0x80) == 0) return 1;           // 0xxxxxxx (1 byte)
    if ((c & 0xE0) == 0xC0) return 2;        // 110xxxxx (2 bytes)
    if ((c & 0xF0) == 0xE0) return 3;        // 1110xxxx (3 bytes)
    if ((c & 0xF8) == 0xF0) return 4;        // 11110xxx (4 bytes)
    return 1;                                 // invalid, treat as 1 byte
}

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

void changeFileElements(std::vector<fileElements>& fileElementsBuffer,int activeBufferIndex, int changingToIndex, int& lastModifiedTime, bool& unsavedChanges , cursorElement& cursor , SelectionElements& selection){
    fileElementsBuffer[activeBufferIndex].lastModified = lastModifiedTime;
    fileElementsBuffer[activeBufferIndex].isChanged = unsavedChanges;
    fileElementsBuffer[activeBufferIndex].selection = selection;
    fileElementsBuffer[activeBufferIndex].cursor = cursor;
    lastModifiedTime = fileElementsBuffer[changingToIndex].lastModified;
    unsavedChanges = fileElementsBuffer[changingToIndex].isChanged;
    selection = fileElementsBuffer[changingToIndex].selection;
    cursor = fileElementsBuffer[changingToIndex].cursor;
}
void SetInfileElements(std::vector<fileElements>& fileElementsBuffer, int Index , int& lastModifiedTime, bool& unsavedChanges, SelectionElements& selection, cursorElement cursor) {
    lastModifiedTime = fileElementsBuffer[Index].lastModified;
    unsavedChanges = fileElementsBuffer[Index].isChanged;
    selection = fileElementsBuffer[Index].selection;
    cursor = fileElementsBuffer[Index].cursor;
}

void detectLanguage(std::vector<std::string>& buffer, std::string& detectedLang, std::string filename){
    if (endswith(filename, ".sh")){
        detectedLang = "bash";
        return;
    }
    std::string firstLine = buffer[0];    
    if (stringContainsString(firstLine, "#!/bin/bash")|| stringContainsString(firstLine, "#!/bin/sh")){
        
        detectedLang = "bash";
    }
    debugWrite("first Line:"+ buffer[0]);
    if (detectedLang != ""){
        debugWrite("Detected language: " + detectedLang);
    }
}

std::string getPossibleCompleteChar(char givenChar , std::vector<char>& openCharList){
    std::string backString = "";
    std::vector<char> correspondingCharStart = {'(','{','[', '"', '\''};
    std::vector<char> correspondingCharEnd = {')','}',']', '"', '\''};
    backString.append(std::to_string(givenChar));
    if (openCharList.size() > 0){
        for (int i = 0; i < openCharList.size(); i++){
            char activeChar = openCharList[i];
            if (activeChar == givenChar){
                return backString;
            }
        }
        // getcharcorrespondingnum
        int charCorrespondingNum;
        for (int i = 0; i < correspondingCharStart.size(); i++){
            if (correspondingCharStart[i] == givenChar){
                charCorrespondingNum = i;
                break;
            }
        }
        backString.append(std::to_string(correspondingCharEnd[charCorrespondingNum]));
        return backString;
    }
    else{
        int charCorrespondingNum;
        for (int i = 0; i < correspondingCharStart.size(); i++){
            if (correspondingCharStart[i] == givenChar){
                charCorrespondingNum = i;
                break;
            }
        }
        backString.append(std::to_string(correspondingCharEnd[charCorrespondingNum]));
        return backString;
    }

}

char getClosingChar(char openChar) {
    static const std::vector<char> openChars = {'(', '{', '[', '"', '\''};
    static const std::vector<char> closeChars = {')', '}', ']', '"', '\''};
    
    for (size_t i = 0; i < openChars.size(); i++) {
        if (openChars[i] == openChar) {
            return closeChars[i];
        }
    }
    return '\0';
}

bool isOpeningChar(char c) {
    return c == '(' || c == '{' || c == '[' || c == '"' || c == '\'';;
}

void updateOpenCharList(std::vector<std::string> buffer, std::vector<char>& openCharList, const cursorElement& cursor){
    std::vector<char> correspondingCharStart = {'(','{','[', '"', '\''};
    std::vector<char> correspondingCharEnd = {')','}',']', '"', '\''};
    for (int i = 0; i < correspondingCharStart.size(); i++){
        char activeCharStart = correspondingCharStart[i];
        char activeCharEnd = correspondingCharEnd[i];
        if (buffer[cursor.Y][cursor.X] == activeCharStart){
            openCharList.push_back(activeCharStart);
            break;
        }
        else if (buffer[cursor.Y][cursor.X] == activeCharEnd){
            for (int j = 0; j < openCharList.size(); j++){
                if (openCharList[j] == activeCharEnd){
                    openCharList.erase(openCharList.begin() + j);
                    break;
                }
            }
            break;
        }
    }
}

void displayInlineSuggestion(const std::vector<std::string>& inlineBuffer,
                             int inlineBufferPosX, int inlineBufferPosY,
                             int cursorXFunc, int cursorYFunc, int rowOffset, int colOffset, int lineNumberWidth) {
                                // Using a template literal – simplest, most readable
                    //debugWrite("DisplayInlineSuggestion Called with posX: " + std::to_string(inlineBufferPosX) + " and posY: " + std::to_string(inlineBufferPosY));

    // Convert file coordinates to screen coordinates
    int screenCursorY = cursorYFunc - rowOffset + 1;
    int screenCursorX = lineNumberWidth + (cursorXFunc - colOffset);
    
    int inlineLineY = screenCursorY;
    int inlineLineX = screenCursorX;

    attron(COLOR_PAIR(10));

    for (size_t i = 0; i < inlineBuffer.size(); i++) {
        for (size_t j = 0; j < inlineBuffer[i].size(); j++) {
            char c = inlineBuffer[i][j];

            if (c == '\n') {
                inlineLineY++;
                inlineLineX = screenCursorX;
                continue;
            }

            // Bounds checking to avoid drawing outside the window
            if (inlineLineY >= 0 && inlineLineY < LINES - 1 && inlineLineX >= 0 && inlineLineX < COLS) {
                mvaddch(inlineLineY, inlineLineX, c);
            }
            inlineLineX++;
        }

        inlineLineY++;
        inlineLineX = screenCursorX;
    }

    attroff(COLOR_PAIR(10));
}

static std::wstring utf8_to_wstring(const std::string &s) {
    if (s.empty()) return L"";

    try {
        std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter;
        return converter.from_bytes(s);
    } catch (const std::range_error&) {
        // Fallback: convert as best we can, replacing invalid bytes
        std::wstring result;
        for (unsigned char c : s) {
            result += static_cast<wchar_t>(c);
        }
        return result;
    }
}



bool checkFileExistance(const std::string& filePath) {
    std::ifstream file(filePath);
    return file.good();
}

void createNewFileFunc(const std::string &filename) {
    std::ofstream out(filename, std::ios::out | std::ios::trunc);
    if (!out) {
        throw std::system_error(errno, std::generic_category(), "Failed to create file: " + filename);
    }
}

void loadFile(const std::string& filename, std::vector<std::string>& targetBuffer, std::vector<std::string>& initialFileBuffer, int& lastModifiedTime) {
    if (!checkFileExistance(filename)) {
        debugWrite("file does not exist");
        targetBuffer.clear();
        targetBuffer.emplace_back();
        initialFileBuffer = targetBuffer; // Track initial state
        lastModifiedTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        return;
    }

    std::ifstream file(filename);
    if (!file) {
        targetBuffer.clear();
        initialFileBuffer = targetBuffer; // Track initial state
        lastModifiedTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        return;
    }

    targetBuffer.clear();
    std::string line;
    while (std::getline(file, line)) {
        targetBuffer.push_back(line);
    }
    if (targetBuffer.empty()) targetBuffer.push_back("");
    
    initialFileBuffer = targetBuffer; // Track initial state after loading

    try {
        lastModifiedTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    } catch (...) {
        lastModifiedTime = 0;
    }
}

void saveFile(const std::string& filename , int& lastModifiedTime, bool& unsavedChanges,
    std::vector<std::string>& initialFileBuffer, int& savedCacheIndex, std::vector<std::string>& buffer,
    int& cacheIndex) {
    //createNewFileFunc(filename);
    debugWrite("Saving file: " + filename);
    if (checkFileExistance(filename) == false){
        createNewFileFunc(filename);
    }
    std::ofstream file(filename);
    for (auto& line : buffer) file << line << "\n";
    
    lastModifiedTime = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()); // time_t
    unsavedChanges = false;
    initialFileBuffer = buffer; // Update initial state to match saved content
    savedCacheIndex = cacheIndex; // Track the cache state when file is saved
}

void copyClipboard(int startY , int endY, std::vector<std::string>& buffer, std::string& clipboard, SelectionElements& selection){
            for (int y = startY; y <= endY; y++) {
                int lineStartX = (y == selection.startY) ? std::min(selection.startX, selection.endX) : 0;
                int lineEndX   = (y == selection.endY) ? std::max(selection.startX, selection.endX) : buffer[y].size();
                clipboard += buffer[y].substr(lineStartX, lineEndX - lineStartX);
                if (y != endY) clipboard += "\n";
            }
            debugWrite("copied to clipboard: " + clipboard);
}

void pasteClipboard(int& cursorY, int& cursorX, std::vector<std::string>& buffer, std::string& clipboard){
    if (clipboard.empty()) return;

    size_t prev = 0, pos;
    std::vector<std::string> clipLines;

    // Split clipboard into lines
    while ((pos = clipboard.find('\n', prev)) != std::string::npos) {
        clipLines.push_back(clipboard.substr(prev, pos - prev));
        prev = pos + 1;
    }
    clipLines.push_back(clipboard.substr(prev)); // last line

    for (size_t i = 0; i < clipLines.size(); ++i) {
        // Ensure cursorY exists
        while (cursorY >= (int)buffer.size()) buffer.push_back("");

        // Ensure current line has enough columns
        if (cursorX > (int)buffer[cursorY].size()) {
            buffer[cursorY].resize(cursorX, ' ');
        }

        if (i == 0) {
            // Insert first line at current cursor
            buffer[cursorY].insert(cursorX, clipLines[i]);
            cursorX += clipLines[i].size();
        } else {
            // Insert new line below current
            cursorY++;
            buffer.insert(buffer.begin() + cursorY, clipLines[i]);
            cursorX = clipLines[i].size();
        }
    }
}

std::string formatTime(int time) {
    time_t t = time;
    struct tm* tm_info = localtime(&t);
    char buffer[26];
    strftime(buffer, 26, "%Y-%m-%d %H:%M:%S", tm_info);
    return std::string(buffer);
}

void showHelp(std::string version, int lineNumberScheme) {
    erase();  // clear the screen

    // Turn on color
    //attron(COLOR_PAIR(lineNumberScheme));

    // Print help text
    std::string versionString = "Version: " + version;
    mvprintw(0, 0, "Idet Editor - Help");
    mvprintw(2, 0, "strg + s to save");
    mvprintw(3, 0, "strg + c to copy");
    mvprintw(4, 0, "strg + v to paste");
    mvprintw(5, 0, "strg + f to search");
    mvprintw(6, 0, "strg + q to quit");
    mvprintw(7, 0, "F1 to open help");
    mvprintw(8, 0, "F2 to change color scheme");
    mvprintw(9, 0, "F3 to enable/disable selection(in case of shift + arrowkeys not working)");
    mvprintw(10, 0, "F7 to open AI settings");
    mvprintw(11, 0, "Shift + arrowkeys to select text");
    mvprintw(12, 0, "Use --multifile to open mutliple files");    
    mvprintw(13, 0, "strg + f2/f3 to switch between files in multifile mode");
    mvprintw(15, 0, "%s", versionString.c_str());

    // Turn off color
    attroff(COLOR_PAIR(lineNumberScheme));

    // Refresh to show changes
    refresh();

    // Wait for user to press any key
    while (true){
        int keyPressed =     getch();
        if (keyPressed > 0){
            break;
        }
    }

}

void tabOverlay(tabOverlayParams& tabOverlayParamsIn);

void draw(const cursorElement& cursor, int& rowOffset, 
    const std::string& filename,int lineNumberScheme, 
    int contentScheme, bool unsavedChanges, 
    int& colOffset, int inlineSuggestionNPredict , bool multiFileMode,
    std::vector<std::string> fileList, int activeBufferIndex, std::string detectedLang,
    std::vector<std::string>& buffer, 
    bool& showInlineSuggestion,
    int lastModifiedTime, bool tabOverlayActive, tabOverlayParams tabParams,
    std::vector<std::string> inlineBuffer, int inlineBufferPosX, int inlineBufferPosY, SelectionElements& selection)
{
erase();
if(detectedLang == "bash"){
    commentPositions.clear(); // Clear comment positions for this draw cycle
}
int lineNumberWidth = std::to_string(buffer.size()).length() + 2;
int maxRows = LINES - 2; // leave last line for status bar
int visibleWidth = COLS - lineNumberWidth;
if (visibleWidth < 1) visibleWidth = 1;

int cursorY = cursor.Y;
int cursorX = cursor.X;
if (cursorY < 0) cursorY = 0;
if (cursorY >= (int)buffer.size()) cursorY = (int)buffer.size() - 1;
if (cursorY < rowOffset) {
    rowOffset = cursorY;
} else if (cursorY >= rowOffset + maxRows) {
    rowOffset = cursorY - maxRows + 1;
}
if (rowOffset < 0) rowOffset = 0;

int maxRowOffset = std::max(0, (int)buffer.size() - maxRows);
if (rowOffset > maxRowOffset) rowOffset = maxRowOffset;
if (cursorX < 0) cursorX = 0;
int lineLen = (cursorY >= 0 && cursorY < (int)buffer.size()) ? (int)buffer[cursorY].size() : 0;
if (cursorX > lineLen) cursorX = lineLen;

if (cursorX < colOffset) {
    colOffset = cursorX;
} else if (cursorX >= colOffset + visibleWidth) {
    colOffset = cursorX - visibleWidth + 1;
}
if (colOffset < 0) colOffset = 0;

int maxLineLen = 0;
for (int i = rowOffset; i < (int)buffer.size() && i < rowOffset + maxRows; ++i)
    if ((int)buffer[i].size() > maxLineLen) maxLineLen = (int)buffer[i].size();
int maxColOffset = std::max(0, maxLineLen - visibleWidth + 1);
if (colOffset > maxColOffset) colOffset = maxColOffset;

// --- HEADER ---
attron(A_BOLD);
mvhline(0, 0, ' ', COLS); 
if (multiFileMode) {
mvprintw(0, 0, "Idet-Editor - File: %s%s | Selection: %s | Suggestion Length: %d | File: %d/%ld",
         filename.c_str(),
         unsavedChanges ? "*" : "",
         selection.active ? "ON" : "OFF", 
        inlineSuggestionNPredict,
        activeBufferIndex + 1,
        fileList.size()
        );
    }else{
mvprintw(0, 0, "Idet-Editor - File: %s%s | Selection: %s | Suggestion Length: %d",
         filename.c_str(),
         unsavedChanges ? "*" : "",
         selection.active ? "ON" : "OFF", 
        inlineSuggestionNPredict
        );
    }
attroff(A_BOLD);
int selTop = std::min(selection.startY, selection.endY);
int selBottom = std::max(selection.startY, selection.endY);
for (int i = 0; i < maxRows && (rowOffset + i) < (int)buffer.size(); ++i) {
    int fileLine = rowOffset + i;

    // --- LINE NUMBERS ---
    attron(COLOR_PAIR(lineNumberScheme));
    mvhline(i + 1, 0, ' ', lineNumberWidth); 
    mvprintw(i + 1, 0, "%*d", lineNumberWidth - 1, fileLine + 1);
    attroff(COLOR_PAIR(lineNumberScheme));

    // --- FILE CONTENT BACKGROUND ---
    attron(COLOR_PAIR(contentScheme));
    mvhline(i + 1, lineNumberWidth, ' ', COLS - lineNumberWidth); 
    attroff(COLOR_PAIR(contentScheme));

    if (fileLine >= (int)buffer.size()) continue;
    std::string& line = buffer[fileLine];

    // Detect syntax highlighting affiliation for this line
    if (detectedLang == "bash"){
        syntaxHighlightingAffiliation.clear();
        detectInLineAffiliation(line, fileLine);
    }
    std::wstring wline = utf8_to_wstring(line);

    int startX = colOffset;
    int endX = std::min((int)wline.size(), colOffset + visibleWidth);

    if (startX >= endX) continue;

    for (int x = startX; x < endX; ++x) {
        int screenX = lineNumberWidth + (x - colOffset);
        bool inSelection = false;

        if (selection.active) {
            if (fileLine > selTop && fileLine < selBottom) inSelection = true;
            else if (fileLine == selTop && fileLine == selBottom)
                inSelection = (x >= std::min(selection.startX, selection.endX) && x < std::max(selection.startX, selection.endX));
            else if (fileLine == selTop)
                inSelection = (x >= selection.startX);
            else if (fileLine == selBottom)
                inSelection = (x < selection.endX);
        }

        // Determine color based on syntax highlighting
        int colorPair = contentScheme; // default content color
        
        // Check if in comment - if so, use comment color regardless of content
        if (detectedLang == "bash"){
        if (isInComment(x, fileLine)) {
            colorPair = 15; // comments (gray/dark)
        } else if (isCommand(x, fileLine)) {
            colorPair = 11; // commands (green)
        } else if (isKeyword(x, fileLine)) {
            colorPair = 12; // keywords (cyan)
        } else if (isInScriptDefinition(x, fileLine)) {
            colorPair = 13; // function definitions (yellow)
        } else if (isOperatorAt(x, fileLine)) {
            colorPair = 14; // operators (magenta)
        }
        }

        attron(COLOR_PAIR(colorPair));
        if (inSelection) attron(A_REVERSE);

        // Print ONE wide character
        mvaddnwstr(i + 1, screenX, &wline[x], 1);

        if (inSelection) attroff(A_REVERSE);
        attroff(COLOR_PAIR(colorPair));
    }
}

// --- STATUS BAR ---
attron(A_REVERSE);
mvhline(LINES - 1, 0, ' ', COLS); // fill status bar
mvprintw(LINES - 1, 0, "CTRL+S=Save | CTRL+Q=Quit | F7 AI-Settings | Line %d/%d | Column %d/%d | Last Modified: %s",
         cursorY + 1, (int)buffer.size(),
         cursorX + 1, (int)buffer[cursorY].size() + 1,
         formatTime(lastModifiedTime).c_str());
attroff(A_REVERSE);

// Move cursor: translate cursorX to screen using colOffset
int screenCursorY = cursorY - rowOffset + 1;
int screenCursorX = lineNumberWidth + (cursorX - colOffset);
if (screenCursorY < 1) screenCursorY = 1;
if (screenCursorY > LINES - 2) screenCursorY = LINES - 2;
if (screenCursorX < lineNumberWidth) screenCursorX = lineNumberWidth;
if (screenCursorX >= COLS) screenCursorX = COLS - 1;
move(screenCursorY, screenCursorX);

// Draw inline suggestion if active
if (showInlineSuggestion && !inlineBuffer.empty()) {
    displayInlineSuggestion(inlineBuffer, inlineBufferPosX, inlineBufferPosY, cursorX, cursorY, rowOffset, colOffset, lineNumberWidth);
}

// Draw tab overlay if active
if (tabOverlayActive && tabParams.exists) {
    tabOverlay(tabParams);
}

refresh();

}

void initLlama() {
    
    //modelLoaded = llama.load_model(modelPath);
    //if (!modelLoaded) {
    //    debugWrite("Failed to load Llama model. Check the path! Used path: " + modelPath);
    //} else {
    //    debugWrite("Llama model loaded successfully. Used model path: " + modelPath);
    //}
//
    //if (checkFileExistance(modelPath)) {
    //    debugWrite("Model file exists");
    //} else {
    //    debugWrite("Model file does not exist!: " + modelPath);
    //}
}

void remove_at(char *buf, int pos) {
    if (pos < 0) return;
    int i = pos;
    while (buf[i] != '\0') {
        buf[i] = buf[i+1];
        i++;
    }
}

std::size_t char_to_byte_index(const std::string &s, std::size_t char_idx) {
    // Handle invalid/out-of-bounds input
    if (char_idx == static_cast<std::size_t>(-1) || char_idx > s.size() * 4) {
        return 0;
    }
    
    std::size_t bytes = 0, chars = 0;
    while (bytes < s.size() && chars < char_idx) {
        unsigned char c = static_cast<unsigned char>(s[bytes]);
        if ((c & 0x80) == 0) bytes += 1;
        else if ((c & 0xE0) == 0xC0) bytes += 2;
        else if ((c & 0xF0) == 0xE0) bytes += 3;
        else if ((c & 0xF8) == 0xF0) bytes += 4;
        else return bytes; // invalid byte -> stop
        ++chars;
    }
    return bytes;
}

void appendCacheActionBuffer(const std::vector<std::string>& oldBuffer, const std::vector<std::string>& newBuffer, 
                             int keyPressed, int cursorX, int cursorY, std::vector<cacheAction>& cacheActionBuffer, 
                             int maxCacheNum,int cacheIndex, int pasteSize = 0) {
    if (cacheIndex >= 0 && cacheIndex < (int)cacheActionBuffer.size() - 1) {
        debugWrite("CACHE: clearing redo history from index " + std::to_string(cacheIndex + 1));
        cacheActionBuffer.erase(cacheActionBuffer.begin() + cacheIndex + 1, cacheActionBuffer.end());
    }
    
    
    cacheAction diff = createDiff(oldBuffer, newBuffer, cursorX, cursorY, keyPressed, pasteSize);
    
    debugWrite("CACHE: createDiff returned: removed=" + std::to_string(diff.removedLines.size()) + 
             ", inserted=" + std::to_string(diff.insertedLines.size()) + 
             ", affectedStartLine=" + std::to_string(diff.affectedStartLine));
    
    
    if (!diff.removedLines.empty() || !diff.insertedLines.empty()) {
        debugWrite("CACHE: adding action to buffer (now size " + std::to_string(cacheActionBuffer.size() + 1) + ")");
        if (cacheActionBuffer.size() >= maxCacheNum) {
            cacheActionBuffer.erase(cacheActionBuffer.begin()); // remove oldest
            cacheIndex = std::max(-1, cacheIndex - 1); // adjust index if we removed the first element
        }
        cacheActionBuffer.push_back(diff);
        cacheIndex = cacheActionBuffer.size() - 1; // always move to latest state after new action
        debugWrite("CACHE: cacheIndex now = " + std::to_string(cacheIndex));
    } else {
        debugWrite("CACHE: no change detected, not caching");
    }
    debugWrite(" Cache Index: " + std::to_string(cacheIndex) + " / " + std::to_string(cacheActionBuffer.size()));
}

void generateEmptyCacheAction(std::vector<cacheAction>& cacheActionBuffer, int& cacheIndex) {
    cacheAction emptyAction;
    emptyAction.affectedStartLine = 0;
    cacheActionBuffer.push_back(emptyAction);
    cacheIndex = cacheActionBuffer.size() - 1;
}
void displayAISettings(const cursorElement& cursor, int& rowOffset,
     const std::string& filename, int lineNumberScheme, int contentScheme,
      bool selectionActive, bool unsavedChanges, int& colOffset, AiProps& AiSettings){
    int selectedSetting = 0;
    const int NUM_SETTINGS = 7;
    bool editingMode = false;
    std::string editBuffer = "";
    
    while (true) {
        erase();
        attron(COLOR_PAIR(lineNumberScheme));
        
        // Print settings with highlighting
        mvprintw(1, 0, "%s AI provider: %s", selectedSetting == 0 ? ">" : " ", AiSettings.AiProvider.c_str());
        mvprintw(2, 0, "%s auth-key: %s", selectedSetting == 1 ? ">" : " ", AiSettings.authToken.empty() ? "(none)" : "(set)");
        mvprintw(3, 0, "%s AI Host: %s", selectedSetting == 2 ? ">" : " ", AiSettings.llamaCompletionHost.c_str());
        mvprintw(4, 0, "%s AI n_predict: %s", selectedSetting == 3 ? ">" : " ", AiSettings.llamaCompletionNPredict.c_str());
        mvprintw(5, 0, "%s model (only ollama): %s", selectedSetting == 4 ? ">" : " ", AiSettings.ollamaModel.c_str());
        mvprintw(6, 0, "%s inline suggestion tokens: %d", selectedSetting == 5 ? ">" : " ", AiSettings.inlineSuggestionNPredict);
        mvprintw(7, 0, "%s auto suggestion delay: %d seconds", selectedSetting == 6 ? ">" : " ", AiSettings.AUTO_SUGGESTION_DELAY);
        
        if (editingMode) {
            mvprintw(9, 0, "Editing: ");
            attron(A_UNDERLINE);
            mvprintw(9, 9, "%s", editBuffer.c_str());
            attroff(A_UNDERLINE);
            mvprintw(10, 0, "[Enter] Save  [Esc] Cancel");
        } else {
            mvprintw(9, 0, "[Arrow Keys] Navigate  [Enter] Edit  [Esc] Exit");
        }
        
        attroff(COLOR_PAIR(lineNumberScheme));
        refresh();
        
        int settingsCh = getch();
        
        if (editingMode) {
            // In editing mode
            if (settingsCh == 10 || settingsCh == 13) { // Enter
                // Save the new value based on selected setting
                switch (selectedSetting) {
                    case 0: // AI Provider
                        if (!editBuffer.empty()) AiSettings.AiProvider = editBuffer;
                        break;
                    case 1: // Auth Token
                        AiSettings.authToken = editBuffer;
                        break;
                    case 2: // Llama Host
                        if (!editBuffer.empty()) AiSettings.llamaCompletionHost = editBuffer;
                        break;
                    case 3: // Llama n_predict
                        if (!editBuffer.empty()) AiSettings.llamaCompletionNPredict = editBuffer;
                        break;
                    case 4: // Ollama Model
                        if (!editBuffer.empty()) AiSettings.ollamaModel = editBuffer;
                        break;
                    case 5: // Inline Suggestion Tokens
                        try {
                            AiSettings.inlineSuggestionNPredict = std::stoi(editBuffer);
                        } catch (...) {}
                        break;
                    case 6: // Auto Suggestion Delay
                        try {
                            AiSettings.AUTO_SUGGESTION_DELAY = std::stoi(editBuffer);
                        } catch (...) {}
                        break;
                }
                editingMode = false;
                editBuffer = "";
            } else if (settingsCh == 27) { // Esc
                editingMode = false;
                editBuffer = "";
            } else if (settingsCh == 127 || settingsCh == KEY_BACKSPACE) { 
                if (!editBuffer.empty()) {
                    editBuffer.pop_back();
                }
            } else if (settingsCh >= 32 && settingsCh <= 126) { // Printable ASCII
                editBuffer += static_cast<char>(settingsCh);
            }
        } else {
            // Navigation mode
            if (settingsCh == KEY_UP) {
                selectedSetting = (selectedSetting - 1 + NUM_SETTINGS) % NUM_SETTINGS;
            } else if (settingsCh == KEY_DOWN) {
                selectedSetting = (selectedSetting + 1) % NUM_SETTINGS;
            } else if (settingsCh == 10 || settingsCh == 13) { // Enter
                editingMode = true;
                // Load current value into edit buffer
                switch (selectedSetting) {
                    case 0:
                        editBuffer = AiSettings.AiProvider;
                        break;
                    case 1:
                        editBuffer = AiSettings.authToken;
                        break;
                    case 2:
                        editBuffer = AiSettings.llamaCompletionHost;
                        break;
                    case 3:
                        editBuffer = AiSettings.llamaCompletionNPredict;
                        break;
                    case 4:
                        editBuffer = AiSettings.ollamaModel;
                        break;
                    case 5:
                        editBuffer = std::to_string(AiSettings.inlineSuggestionNPredict);
                        break;
                    case 6:
                        editBuffer = std::to_string(AiSettings.AUTO_SUGGESTION_DELAY);
                        break;
                }
            } else if (settingsCh == 27) { // Esc
                break; // Exit settings menu
            }
        }
    }
}


void loadConfig(std::string configPath, AiProps AiSettings) {
    std::ifstream configFile(configPath);
    if (!configFile) {
        debugWrite("No config file found at " + configPath + ", using defaults.");
        return;
    }

    try {
        nlohmann::json configJson;
        configFile >> configJson;

        if (configJson.contains("AiProvider"))
            AiSettings.AiProvider = configJson["AiProvider"].get<std::string>();
        if (configJson.contains("authToken"))
            AiSettings.authToken = configJson["authToken"].get<std::string>();
        if (configJson.contains("llamaCompletionHost"))
            AiSettings.llamaCompletionHost = configJson["llamaCompletionHost"].get<std::string>();
        if (configJson.contains("llamaCompletionNPredict"))
            AiSettings.llamaCompletionNPredict = configJson["llamaCompletionNPredict"].get<std::string>();

        debugWrite("Config loaded from " + configPath);
    } catch (const std::exception& e) {
        debugWrite(std::string("Failed to parse config file: ") + e.what());
    }
}

std::vector<std::string> generateInlineBuffer(const std::string& inputBufferString) {
    std::vector<std::string> outVector;
    outVector.emplace_back(); // start with first line

    for (char currentChar : inputBufferString) {
        if (currentChar == '\n') {
            outVector.emplace_back(); // new line
        } else {
            outVector.back().push_back(currentChar);
        }
    }
    return outVector;
}

void undo(cursorElement& cursor, std::vector<std::string>& buffer, std::vector<cacheAction>& cacheActionBuffer, int& cacheIndex, int savedCacheIndex,
            std::vector<std::string>& initialFileBuffer, bool& unsavedChanges ) {
    debugWrite("UNDO: cacheActionBuffer.size()=" + std::to_string(cacheActionBuffer.size()) + ", cacheIndex=" + std::to_string(cacheIndex));
    
    if (cacheIndex < 0) {
        debugWrite("Nothing to undo");
        return;
    }
    
    cacheIndex--;
    debugWrite("UNDO: after decrement, cacheIndex=" + std::to_string(cacheIndex));
    
    if (cacheIndex >= 0) {
        const cacheAction& action = cacheActionBuffer[cacheIndex];
        debugWrite("UNDO: applying diffs 0 to " + std::to_string(cacheIndex));
        buffer = initialFileBuffer;
        
        for (int i = 0; i <= cacheIndex; ++i) {
            debugWrite("UNDO: applying diff " + std::to_string(i));
            applyDiff(buffer, cacheActionBuffer[i]);
        }
        
        debugWrite("UNDO: buffer after reconstruction has " + std::to_string(buffer.size()) + " lines");
        if (!buffer.empty()) {
            debugWrite("UNDO: line 0 = '" + buffer[0] + "'");
        }
        
        cursor.X = action.cursorX;
        cursor.Y = action.cursorY;
        unsavedChanges = (cacheIndex != savedCacheIndex);
        debugWrite("Undo: restored to cache index " + std::to_string(cacheIndex));
    } else {
        debugWrite("UNDO: restoring to initial file state");
        buffer = initialFileBuffer;
        cursor.X = 0;
        cursor.Y = 0;
        unsavedChanges = (savedCacheIndex != -1);
        debugWrite("Undo: restored to initial state, buf size=" + std::to_string(buffer.size()));
    }
}

void redo(cursorElement& cursor, std::vector<std::string>& buffer, std::vector<cacheAction>& cacheActionBuffer, int& cacheIndex, int savedCacheIndex,
        std::vector<std::string>& initialFileBuffer, bool& unsavedChanges) {
    if (cacheIndex >= (int)cacheActionBuffer.size() - 1) {
        debugWrite("Nothing to redo");
        return;
    }
    
    cacheIndex++;
    if (cacheIndex < (int)cacheActionBuffer.size()) {
        const cacheAction& action = cacheActionBuffer[cacheIndex];
        // Reconstruct buffer by starting from initial file state and applying all diffs from 0 to cacheIndex
        buffer = initialFileBuffer;
        
        for (int i = 0; i <= cacheIndex; ++i) {
            applyDiff(buffer, cacheActionBuffer[i]);
        }
        
        cursor.X = action.cursorX;
        cursor.Y = action.cursorY;
        // Only mark as unsaved if we're not at the saved state
        unsavedChanges = (cacheIndex != savedCacheIndex);
        debugWrite("Redo: restored to cache index " + std::to_string(cacheIndex));
    }
}

void getInlineSuggestion(const cursorElement& cursor, std::vector<std::string>& buffer, int maxInlineSuggestionPromptLength, AiProps AiSettings,
        std::vector<std::string>& inlineBuffer, int& inlineBufferPosX, int& inlineBufferPosY, bool& showInlineSuggestion,  bool otherConstruct = false){
        debugWrite("Tab pressed - Triggering AI Completion");
        int cursorX = cursor.X;
        int cursorY = cursor.Y;
        std::vector<std::string> vectorBeforetxt;
        
        vectorBeforetxt.reserve(static_cast<size_t>(cursorY) + 1); // avoid reallocs
        
        int limitLine = std::max(0, cursorY); // ensure non-negative
        for (int vecLine = 0; vecLine < limitLine && vecLine < static_cast<int>(buffer.size()); ++vecLine) {
            // check if vecBeforetxt is already at max prompt length
            if (getUtf8StrLen(joinVecLines(vectorBeforetxt)) >= maxInlineSuggestionPromptLength) {
                otherConstruct = true;
                break;
            }
            vectorBeforetxt.push_back(buffer[vecLine]);
        }
        vectorBeforetxt.clear();
        if (otherConstruct) {
            std::reverse(vectorBeforetxt.begin(), vectorBeforetxt.end());
            for (int vecLine = cursorY; vecLine >= 0 && vecLine < static_cast<int>(buffer.size()); --vecLine) {
                if (getUtf8StrLen(joinVecLines(vectorBeforetxt)) >= maxInlineSuggestionPromptLength) {
                    break;
                }
                vectorBeforetxt.push_back(buffer[vecLine]);
            }
            std::reverse(vectorBeforetxt.begin(), vectorBeforetxt.end());
        }


        std::string charsBefore;
        if (cursorY >= 0 && cursorY < static_cast<int>(buffer.size())) {
            std::size_t bytePos = char_to_byte_index(buffer[cursorY], cursorX);
            charsBefore = buffer[cursorY].substr(0, bytePos);
        } // else charsBefore stays empty

        vectorBeforetxt.push_back(charsBefore);

        // Join with commas
        std::string StrVecTxt;
        StrVecTxt.reserve(vectorBeforetxt.size() * 8);
        for (size_t i = 0; i < vectorBeforetxt.size(); ++i) {
            if (i) StrVecTxt.push_back(',');
            StrVecTxt += vectorBeforetxt[i];
        }
        debugWrite("vector: " + StrVecTxt);
        std::string promptText = getStingFromVec(vectorBeforetxt);
        debugWrite("promptText: " + promptText);
        std::string llamaOutput = AiCompletion(promptText, (AiSettings.llamaCompletionHost), std::to_string(AiSettings.inlineSuggestionNPredict),
                                   [](const std::string& msg){ debugWrite(msg); }, AiSettings.AiProvider, AiSettings.ollamaModel);
        debugWrite("LlamaOutput is: " + llamaOutput);
        // Store inline buffer and set flag to display on next draw
        inlineBuffer = generateInlineBuffer(llamaOutput);
        inlineBufferPosX = cursorX + 1;
        inlineBufferPosY = cursorY;
        showInlineSuggestion = true;
}

void loadInfileElements(std::vector<fileElements>& fileElementsBuffer, std::string handleFile) {
    struct stat fileInfo;
    fileElements tmpFileElement;
    if (stat(handleFile.c_str(), &fileInfo) == 0) {
        
        tmpFileElement.lastModified = fileInfo.st_mtime;

    } else {
        std::cerr << "Error: Could not retrieve file information for " << handleFile << std::endl;
        tmpFileElement.lastModified = 0;
    }
    tmpFileElement.isChanged = false;
    tmpFileElement.selEndX = 0;
    tmpFileElement.selEndY = 0;
    tmpFileElement.selStartX = 0;
    tmpFileElement.selStartY = 0;
    tmpFileElement.cursorX = 0;
    tmpFileElement.cursorY = 0;
    fileElementsBuffer.push_back(tmpFileElement);
}

void changeActiveBuffer(
    std::vector<std::vector<std::string>>& inactiveBuffer,
    std::vector<std::string>& activeBuffer,
    int& currentActiveIndex,   
    int newActiveIndex
) {
    if (newActiveIndex < 0 || newActiveIndex >= (int)inactiveBuffer.size()) {
        debugWrite("Invalid buffer index: " + std::to_string(newActiveIndex));
        return;
    }


    if (currentActiveIndex >= 0 && currentActiveIndex < (int)inactiveBuffer.size()) {
        inactiveBuffer[currentActiveIndex] = std::move(activeBuffer);
    }

    activeBuffer = std::move(inactiveBuffer[newActiveIndex]);

    inactiveBuffer[newActiveIndex].clear();

    currentActiveIndex = newActiveIndex;

    debugWrite("Switched to buffer index: " + std::to_string(newActiveIndex));
}

void reloadFile(std::string filename, std::vector<std::string>& buffer , std::vector<std::string>& initialFileBuffer, int& lastModifiedTime , std::vector<cacheAction>& cacheActionBuffer, int& cacheIndex, int& savedCacheIndex){
    buffer.clear();
    loadFile(filename,buffer, initialFileBuffer, lastModifiedTime);
    // Reset undo/redo history when reloading file
    cacheActionBuffer.clear();
    cacheIndex = -1;
    savedCacheIndex = -1;
}

void tabOverlay(tabOverlayParams& tabOverlayParamsIn) {
    if (!tabOverlayParamsIn.exists) {
        return;
    }
    if (tabOverlayParamsIn.cachedCursorX != tabOverlayParamsIn.cursorX || 
        tabOverlayParamsIn.cachedCursorY != tabOverlayParamsIn.cursorY) {
        tabOverlayParamsIn.needsUpdate = true;
    }
    if (tabOverlayParamsIn.needsUpdate && !tabOverlayParamsIn.buffer.empty()) {
        if (tabOverlayParamsIn.cursorY >= tabOverlayParamsIn.buffer.size()) {
            return;
        }
        std::string lineContent = tabOverlayParamsIn.buffer[tabOverlayParamsIn.cursorY];
        std::string compareString = beforeCursor(lineContent, tabOverlayParamsIn.cursorX);
        if (compareString != tabOverlayParamsIn.cachedCompareString) {
            posCords closestThingCords = findInBuffer(tabOverlayParamsIn.buffer, compareString, 1, 0);
            std::string closeWord = getWordFromCords(lineContent, closestThingCords);
            if (!closeWord.empty() && closeWord != compareString) {
                tabOverlayParamsIn.cachedWord = closeWord;
                tabOverlayParamsIn.cachedCompareString = compareString;
                tabOverlayParamsIn.cachedCursorX = tabOverlayParamsIn.cursorX;
                tabOverlayParamsIn.cachedCursorY = tabOverlayParamsIn.cursorY;
                tabOverlayParamsIn.needsUpdate = false;
                debugWrite("Tab Overlay - Found new word: " + closeWord);
            } else {
                tabOverlayParamsIn.cachedWord = "";
                tabOverlayParamsIn.exists = false;
                return;
            }
        } else {
            tabOverlayParamsIn.needsUpdate = false;
        }
    }
    if (!tabOverlayParamsIn.cachedWord.empty()) {
        attron(COLOR_PAIR(5));
        int y = tabOverlayParamsIn.cursorY + 2;
        int x = tabOverlayParamsIn.cursorX;
        
        if (y >= 0 && y < LINES && x >= 0 && x < COLS) {
            mvprintw(y, x, "%s", tabOverlayParamsIn.cachedWord.c_str());
        }
        attroff(COLOR_PAIR(5));
        debugWrite("Tab Overlay - Displaying cached word: " + tabOverlayParamsIn.cachedWord);
    }
}
