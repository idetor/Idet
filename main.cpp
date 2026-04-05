#include <locale.h>
#include <cwchar>
#include <vector>
#include <string>
#include <ncurses.h>
#include <codecvt>
#include <stdexcept>
#include <fstream>
#include <algorithm>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <string_view>
#include <iostream>
#include <unistd.h>
//#include "headers/LlamaClient.hpp"
#include "headers/networkAIApi.hpp"
#include "headers/functions.h"


const std::string version = "0.0.0";
std::ofstream debugOut;

void debugWrite(const std::string& msg) {
    if (debugOut.is_open()) {
        debugOut << msg << std::endl;
        debugOut.flush();
    }
}

#define CTRL_KEY(k) ((k) & 0x1f)

// init Vars

// General Vars
int lastModifiedTime = 0;
std::vector<std::string> buffer;
std::vector<std::string> inlineBuffer;
int inlineBufferPosX = 0;
int inlineBufferPosY = 0;
bool selectionActive = false;
int selStartY = 0, selStartX = 0;
int selEndY = 0, selEndX = 0;
std::string clipboard;
int lineNumberScheme = 1; // 1 or 2
int contentScheme = 3;    // 3 or 4
bool unsavedChanges = false;
bool createNewFile = true;
std::string configPath = "~/.config/idet/config.json";
int lastEditTime = 0;
const size_t DEBUG_MAX = 10000;
std::string filename;
std::vector<cacheAction> cacheActionBuffer; 
int maxCacheNum = 100;
int cacheIndex = -1; // tracks current position in undo/redo history (-1 means at latest state)
std::vector<std::vector<std::string>> inactiveBuffer;
bool multiFileMode = false;
std::vector<std::string> fileList;
int activeBufferIndex = 0;
std::vector<char> openCharList;
bool activeSearch = false;
std::string searchTerm = "";
int SearchLastFoundX = -1;
int SearchLastFoundY = -1;
std::vector<posCords> searchResults;
int searchcount = 0;

// AI Vars
std::string modelPath = "/var/models/tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf";
std::string authToken = "";
std::string llamaCompletionHost = "http://localhost:8080"; //URL of llamacpp
std::string llamaCompletionNPredict = "5"; // how many tokens to generate with TAB
std::string ollamaModel = "gpt-oss:20b";
std::string AiProvider = "llamacpp";
int inlineSuggestionNPredict = 5;
int AUTO_SUGGESTION_DELAY = 3;
int maxInlinePromptSize = 10000;
bool llamaInit = false;
bool modelLoaded = false;
bool showInlineSuggestion = false;
bool inlineSuggestionExists = false;
bool allowInlineSuggestion = true;
bool autoSuggestionTriggered = false;

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

void updateOpenCharList(std::vector<std::string> buffer, std::vector<char>& openCharList, int cursorX, int cursorY){
    std::vector<char> correspondingCharStart = {'(','{','[', '"', '\''};
    std::vector<char> correspondingCharEnd = {')','}',']', '"', '\''};
    for (int i = 0; i < correspondingCharStart.size(); i++){
        char activeCharStart = correspondingCharStart[i];
        char activeCharEnd = correspondingCharEnd[i];
        if (buffer[cursorY][cursorX] == activeCharStart){
            openCharList.push_back(activeCharStart);
            break;
        }
        else if (buffer[cursorY][cursorX] == activeCharEnd){
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

void debugWrite(std::ofstream& out, const std::string& msg) {
    if (!out.is_open()) return;

    if (msg.size() <= DEBUG_MAX) {
        out << msg;
    } else {
        out << msg.substr(0, DEBUG_MAX);
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

void loadFile(const std::string& filename, std::vector<std::string>& targetBuffer = buffer) {
    if (!checkFileExistance(filename)) {
        debugWrite("file does not exist");
        targetBuffer.clear();
        targetBuffer.emplace_back();
        lastModifiedTime = 0;
        return;
    }

    std::ifstream file(filename);
    if (!file) {
        targetBuffer.clear();
        lastModifiedTime = 0;
        return;
    }

    targetBuffer.clear();
    std::string line;
    while (std::getline(file, line)) {
        targetBuffer.push_back(line);
    }
    if (targetBuffer.empty()) targetBuffer.push_back("");

    try {
        auto ftime = std::filesystem::last_write_time(filename);
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - decltype(ftime)::clock::now()
            + std::chrono::system_clock::now()
        );
        lastModifiedTime = std::chrono::system_clock::to_time_t(sctp);
    } catch (...) {
        lastModifiedTime = 0;
    }
}

void saveFile(const std::string& filename ) {
    //createNewFileFunc(filename);
    debugWrite("Saving file: " + filename);
    if (checkFileExistance(filename) == false){
        createNewFileFunc(filename);
    }
    std::ofstream file(filename);
    for (auto& line : buffer) file << line << "\n";
    
    auto ftime = std::filesystem::last_write_time(filename);
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                    ftime - decltype(ftime)::clock::now()
                    + std::chrono::system_clock::now());
    lastModifiedTime = std::chrono::system_clock::to_time_t(sctp); // time_t
    unsavedChanges = false;
}

void copyClipboard(int startY , int endY){
            for (int y = startY; y <= endY; y++) {
                int lineStartX = (y == selStartY) ? std::min(selStartX, selEndX) : 0;
                int lineEndX   = (y == selEndY) ? std::max(selStartX, selEndX) : buffer[y].size();
                clipboard += buffer[y].substr(lineStartX, lineEndX - lineStartX);
                if (y != endY) clipboard += "\n";
            }
            debugWrite("copied to clipboard: " + clipboard);
}

void pasteClipboard(int& cursorY, int& cursorX, std::vector<std::string>& buffer) {
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



void showHelp() {
    erase();  // clear the screen

    // Turn on color
    attron(COLOR_PAIR(lineNumberScheme));

    // Print help text
    mvprintw(1, 0, "help");
    mvprintw(2, 0, "help");
    mvprintw(3, 0, "strg + c to copy");

    // Turn off color
    attroff(COLOR_PAIR(lineNumberScheme));

    // Refresh to show changes
    refresh();

    // Wait for user to press any key
    getch();
}

void draw(int cursorY, int cursorX, int& rowOffset, 
    const std::string& filename,int lineNumberScheme, 
    int contentScheme, bool selectionActive,bool unsavedChanges, 
    int& colOffset, int inlineSuggestionNPredict = 0 , bool multiFileMode = false ,
    std::vector<std::string> fileList = std::vector<std::string>() , int activeBufferIndex = 0)
{
erase();

int lineNumberWidth = std::to_string(buffer.size()).length() + 2;
int maxRows = LINES - 2; // leave last line for status bar
int visibleWidth = COLS - lineNumberWidth;
if (visibleWidth < 1) visibleWidth = 1;

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
         selectionActive ? "ON" : "OFF", 
        inlineSuggestionNPredict,
        activeBufferIndex + 1,
        fileList.size()
        );
    }else{
mvprintw(0, 0, "Idet-Editor - File: %s%s | Selection: %s | Suggestion Length: %d",
         filename.c_str(),
         unsavedChanges ? "*" : "",
         selectionActive ? "ON" : "OFF", 
        inlineSuggestionNPredict
        );
    }
attroff(A_BOLD);
int selTop = std::min(selStartY, selEndY);
int selBottom = std::max(selStartY, selEndY);
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

    
    std::wstring wline = utf8_to_wstring(line);

    int startX = colOffset;
    int endX = std::min((int)wline.size(), colOffset + visibleWidth);

    if (startX >= endX) continue;

    for (int x = startX; x < endX; ++x) {
        int screenX = lineNumberWidth + (x - colOffset);
        bool inSelection = false;

        if (selectionActive) {
            if (fileLine > selTop && fileLine < selBottom) inSelection = true;
            else if (fileLine == selTop && fileLine == selBottom)
                inSelection = (x >= std::min(selStartX, selEndX) && x < std::max(selStartX, selEndX));
            else if (fileLine == selTop)
                inSelection = (x >= selStartX);
            else if (fileLine == selBottom)
                inSelection = (x < selEndX);
        }

        attron(COLOR_PAIR(contentScheme));
        if (inSelection) attron(A_REVERSE);

        // Print ONE wide character
        mvaddnwstr(i + 1, screenX, &wline[x], 1);

        if (inSelection) attroff(A_REVERSE);
        attroff(COLOR_PAIR(contentScheme));
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
                             int maxCacheNum, int pasteSize = 0) {
    // If not at the latest state, remove all "future" actions (redo history gets cleared)
    if (cacheIndex >= 0 && cacheIndex < (int)cacheActionBuffer.size() - 1) {
        cacheActionBuffer.erase(cacheActionBuffer.begin() + cacheIndex + 1, cacheActionBuffer.end());
    }
    
    // Create a diff instead of storing full buffer
    cacheAction diff = createDiff(oldBuffer, newBuffer, cursorX, cursorY, keyPressed, pasteSize);
    
    // Only add to cache if there's actually a change
    if (!diff.removedLines.empty() || !diff.insertedLines.empty()) {
        if (cacheActionBuffer.size() >= maxCacheNum) {
            cacheActionBuffer.erase(cacheActionBuffer.begin()); // remove oldest
            cacheIndex = std::max(-1, cacheIndex - 1); // adjust index if we removed the first element
        }
        cacheActionBuffer.push_back(diff);
        cacheIndex = cacheActionBuffer.size() - 1; // always move to latest state after new action
    }
}

void drawAISettings(std::string authToken, std::string llamaCompletionHost, std::string llamaCompletionNPredict, std::string ollamaModel , std::string AiProvider, int inlineSuggestionNPredict, int AUTO_SUGGESTION_DELAY){
    erase();  // clear the screen

    // Turn on color
    attron(COLOR_PAIR(lineNumberScheme));

    // Print AI settings
    mvprintw(1, 0, "AI Provider: %s", AiProvider.c_str());
    mvprintw(2, 0, "Auth Token: %s", authToken.empty() ? "(none)" : "(set)");
    mvprintw(3, 0, "Llama Host: %s", llamaCompletionHost.c_str());
    mvprintw(4, 0, "Llama n_predict: %s", llamaCompletionNPredict.c_str());
    mvprintw(5, 0, "Ollama Model: %s", ollamaModel.c_str());
    mvprintw(6, 0, "Inline Suggestion Tokens: %d", inlineSuggestionNPredict);
    mvprintw(7, 0, "Auto Suggestion Delay: %d seconds", AUTO_SUGGESTION_DELAY);

    // Turn off color
    attroff(COLOR_PAIR(lineNumberScheme));

    // Refresh to show changes
    refresh();

}

void displayAISettings(int cursorY, int cursorX, int& rowOffset, const std::string& filename,int lineNumberScheme, int contentScheme, bool selectionActive,bool unsavedChanges, int& colOffset, std::string authToken, std::string llamaCompletionHost, std::string llamaCompletionNPredict, std::string ollamaModel , std::string AiProvider, int inlineSuggestionNPredict, int AUTO_SUGGESTION_DELAY){
    int selectedSetting = 0;
    const int NUM_SETTINGS = 7;
    bool editingMode = false;
    std::string editBuffer = "";
    
    while (true) {
        erase();
        attron(COLOR_PAIR(lineNumberScheme));
        
        // Print settings with highlighting
        mvprintw(1, 0, "%s AI Provider: %s", selectedSetting == 0 ? ">" : " ", ::AiProvider.c_str());
        mvprintw(2, 0, "%s Auth Token: %s", selectedSetting == 1 ? ">" : " ", ::authToken.empty() ? "(none)" : "(set)");
        mvprintw(3, 0, "%s Llama Host: %s", selectedSetting == 2 ? ">" : " ", ::llamaCompletionHost.c_str());
        mvprintw(4, 0, "%s Llama n_predict: %s", selectedSetting == 3 ? ">" : " ", ::llamaCompletionNPredict.c_str());
        mvprintw(5, 0, "%s Ollama Model: %s", selectedSetting == 4 ? ">" : " ", ::ollamaModel.c_str());
        mvprintw(6, 0, "%s Inline Suggestion Tokens: %d", selectedSetting == 5 ? ">" : " ", ::inlineSuggestionNPredict);
        mvprintw(7, 0, "%s Auto Suggestion Delay: %d seconds", selectedSetting == 6 ? ">" : " ", ::AUTO_SUGGESTION_DELAY);
        
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
                        if (!editBuffer.empty()) ::AiProvider = editBuffer;
                        break;
                    case 1: // Auth Token
                        ::authToken = editBuffer;
                        break;
                    case 2: // Llama Host
                        if (!editBuffer.empty()) ::llamaCompletionHost = editBuffer;
                        break;
                    case 3: // Llama n_predict
                        if (!editBuffer.empty()) ::llamaCompletionNPredict = editBuffer;
                        break;
                    case 4: // Ollama Model
                        if (!editBuffer.empty()) ::ollamaModel = editBuffer;
                        break;
                    case 5: // Inline Suggestion Tokens
                        try {
                            ::inlineSuggestionNPredict = std::stoi(editBuffer);
                        } catch (...) {}
                        break;
                    case 6: // Auto Suggestion Delay
                        try {
                            ::AUTO_SUGGESTION_DELAY = std::stoi(editBuffer);
                        } catch (...) {}
                        break;
                }
                editingMode = false;
                editBuffer = "";
            } else if (settingsCh == 27) { // Esc
                editingMode = false;
                editBuffer = "";
            } else if (settingsCh == 127 || settingsCh == KEY_BACKSPACE) { // Backspace
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
                        editBuffer = ::AiProvider;
                        break;
                    case 1:
                        editBuffer = ::authToken;
                        break;
                    case 2:
                        editBuffer = ::llamaCompletionHost;
                        break;
                    case 3:
                        editBuffer = ::llamaCompletionNPredict;
                        break;
                    case 4:
                        editBuffer = ::ollamaModel;
                        break;
                    case 5:
                        editBuffer = std::to_string(::inlineSuggestionNPredict);
                        break;
                    case 6:
                        editBuffer = std::to_string(::AUTO_SUGGESTION_DELAY);
                        break;
                }
            } else if (settingsCh == 27) { // Esc
                break; // Exit settings menu
            }
        }
    }
}

void loadConfig(std::string configPath) {
    std::ifstream configFile(configPath);
    if (!configFile) {
        debugWrite("No config file found at " + configPath + ", using defaults.");
        return;
    }

    try {
        nlohmann::json configJson;
        configFile >> configJson;

        if (configJson.contains("AiProvider"))
            AiProvider = configJson["AiProvider"].get<std::string>();
        if (configJson.contains("modelPath"))
            modelPath = configJson["modelPath"].get<std::string>();
        if (configJson.contains("authToken"))
            authToken = configJson["authToken"].get<std::string>();
        if (configJson.contains("llamaCompletionHost"))
            llamaCompletionHost = configJson["llamaCompletionHost"].get<std::string>();
        if (configJson.contains("llamaCompletionNPredict"))
            llamaCompletionNPredict = configJson["llamaCompletionNPredict"].get<std::string>();

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

void undo(int& cursorX, int& cursorY) {
    if (cacheIndex < 0) {
        debugWrite("Nothing to undo");
        return;
    }
    
    cacheIndex--;
    if (cacheIndex >= 0) {
        const cacheAction& action = cacheActionBuffer[cacheIndex];
        // Reconstruct buffer by applying all diffs from start to cacheIndex
        buffer.clear();
        buffer.push_back("");
        
        for (int i = 0; i <= cacheIndex; ++i) {
            applyDiff(buffer, cacheActionBuffer[i]);
        }
        
        cursorX = action.cursorX;
        cursorY = action.cursorY;
        unsavedChanges = true;
        debugWrite("Undo: restored to cache index " + std::to_string(cacheIndex));
    } else {
        // Before any actions - restore to empty buffer
        buffer.clear();
        buffer.push_back("");
        cursorX = 0;
        cursorY = 0;
        unsavedChanges = true;
        debugWrite("Undo: restored to initial state");
    }
}

void redo(int& cursorX, int& cursorY) {
    if (cacheIndex >= (int)cacheActionBuffer.size() - 1) {
        debugWrite("Nothing to redo");
        return;
    }
    
    cacheIndex++;
    if (cacheIndex < (int)cacheActionBuffer.size()) {
        const cacheAction& action = cacheActionBuffer[cacheIndex];
        // Reconstruct buffer by applying all diffs from start to cacheIndex
        buffer.clear();
        buffer.push_back("");
        
        for (int i = 0; i <= cacheIndex; ++i) {
            applyDiff(buffer, cacheActionBuffer[i]);
        }
        
        cursorX = action.cursorX;
        cursorY = action.cursorY;
        unsavedChanges = true;
        debugWrite("Redo: restored to cache index " + std::to_string(cacheIndex));
    }
}

void getInlineSuggestion(int cursorX, int cursorY, std::vector<std::string>& buffer, int maxInlineSuggestionPromptLength, bool otherConstruct = false){
        debugWrite("Tab pressed - Triggering AI Completion");
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
        std::string llamaOutput = AiCompletion(promptText, (llamaCompletionHost), std::to_string(inlineSuggestionNPredict),
                                   [](const std::string& msg){ debugWrite(msg); }, AiProvider, ollamaModel);
        debugWrite("LlamaOutput is: " + llamaOutput);
        // Store inline buffer and set flag to display on next draw
        inlineBuffer = generateInlineBuffer(llamaOutput);
        inlineBufferPosX = cursorX + 1;
        inlineBufferPosY = cursorY;
        showInlineSuggestion = true;
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


int main(int argc, char* argv[]) {
    // Set locale for UTF-8 support
    setlocale(LC_ALL, "");
    std::locale::global(std::locale(""));
    
    if (argc <= 1){
        std::cerr << "No file or parameter given!\n";
        return 1;
    }
    std::string debugTTY;
    // check the early args
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--multiFile") {
            multiFileMode = true;
        }
        if (std::string(argv[i]) == "--config") {
            configPath = argv[i + 1];
        }
    }
    


    // check the args
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "-d" && i + 1 < argc) {
            debugTTY = argv[++i]; // skip value
        }
        else if (arg == "-h" || arg == "--help") {
            printf("Usage: %s <filename> [-d debug_pipe]\n", argv[0]);
            return 0;
        }
        else if (arg == "-v" || arg == "--version") {
            printf("Version %s\n", version.c_str());
            return 0;
        }
        else if (arg == "-p" || arg == "--provider") {
            if (i + 1 < argc) AiProvider = argv[++i];
        }
        else if (arg == "-m" || arg == "--model") {
            if (i + 1 < argc) modelPath = argv[++i];
        }
        else if (arg == "-a" || arg == "--auth") {
            if (i + 1 < argc) authToken = argv[++i];
        }
        else if (arg == "--ollamaModel") {
            if (i + 1 < argc) ollamaModel = argv[++i];
        }
        else if (arg == "-i" || arg == "--inline") {
            if (i + 1 < argc) {
                try {
                    inlineSuggestionNPredict = std::stoi(argv[++i]);
                } catch (...) {}
            }
        }
        else if (arg == "-h" || arg == "--host") {
            if (i + 1 < argc) {
                std::string val = argv[++i];
                if (val.rfind("http", 0) == 0)
                    llamaCompletionHost = val;
                else
                    llamaCompletionHost = "http://" + val;
            }
        }
        else if (arg == "-n" || arg == "--npredict") {
            if (i + 1 < argc) llamaCompletionNPredict = argv[++i];
        }
        else if (arg == "--noNewFile") {
            createNewFile = false;
        }
        else if (arg[0] != '-') {
            
            if (multiFileMode) {
                fileList.push_back(arg);
                //std::cerr << "Adding file: " << arg << "\n";
            } else {
                filename = arg;
            }
        }
    }


    if (multiFileMode){
        if (fileList.empty()){
            std::cerr << "Multi-file mode enabled but no files provided!\n";
            return 1;
        }
        debugWrite("Files to load: " + strVecToString(fileList));
        filename = fileList[0]; // set first file as main buffer file
    }

    if (!debugTTY.empty()) {
    debugOut.open(debugTTY);
    if (!debugOut.is_open()) {
        fprintf(stderr, "Failed to open debug pipe: %s\n", debugTTY.c_str());
    }
    }
    debugWrite("Editor started");
            // init config file
    if (checkFileExistance(configPath)){
            ConfigLoader config(configPath);
            debugWrite("Config file content: " + jsonToString(config.get()));
            loadConfig(configPath);
    }
    else {
        debugWrite("!!!No config file found at " + configPath);
    }
    debugWrite("Loading File: " + filename);
    if (checkFileExistance(filename)){
        loadFile(filename);
    }
    else {

        if (createNewFile == true){
            //createNewFileFunc(argv[1]);
            loadFile(filename);
        }
    }
    // load other files into inactive buffers if multiFileMode is enabled
    if (multiFileMode == true) {
        //leave first entry in inactiveBuffer free for main buffer
        std::vector<std::string> emptyBuffer;

        inactiveBuffer.push_back(emptyBuffer); // main buffer
        for (int i = 1; i < fileList.size(); i++) {
            std::string handlingFile = fileList[i];
            std::vector<std::string> tmpFileBuffer;
            loadFile(handlingFile, tmpFileBuffer);
            inactiveBuffer.push_back(tmpFileBuffer);
        }
    }

    // Initialize ncurses
    initscr();
    start_color();
    use_default_colors();
    init_pair(1, COLOR_BLACK, COLOR_CYAN);   // line numbers default
    init_pair(2, COLOR_WHITE, COLOR_BLUE);   // alternate line numbers
    init_pair(3, COLOR_WHITE, COLOR_BLACK);  // content default
    init_pair(4, COLOR_YELLOW, COLOR_BLACK); // alternate content

    init_pair(10, COLOR_CYAN, COLOR_BLUE);
    init_pair(100, COLOR_WHITE,COLOR_BLACK); 
    init_pair(110, COLOR_BLACK,COLOR_WHITE);
    raw();
    keypad(stdscr, TRUE);
    noecho();
    nodelay(stdscr, TRUE);

    int cursorX = 0, cursorY = 0;
    int rowOffset = 0;
    int colOffset = 0;
    int ch;
    


    // Initialize lastEditTime to current time
    auto initTime = std::chrono::system_clock::now();
    lastEditTime = std::chrono::system_clock::to_time_t(initTime);

    while (true) {
        // Scroll logic
        int maxVisibleRows = LINES - 2;
        if (cursorY - rowOffset >= maxVisibleRows) rowOffset = cursorY - maxVisibleRows + 1;
        if (cursorY - rowOffset < 0) rowOffset = cursorY;

        draw(cursorY, cursorX, rowOffset, filename, lineNumberScheme, contentScheme, selectionActive, unsavedChanges, colOffset, inlineSuggestionNPredict, multiFileMode, fileList, activeBufferIndex);
        if (activeSearch) {
            debugWrite("Searching through results...");
            emptySearchOverlay(searchTerm);
            int ch = waitOnKeyPress();
            debugWrite("Key pressed during search: " + std::to_string(ch));
            if (ch == 10) {
                debugWrite("Enter pressed, moving through results");
                if (!searchResults.empty()) {
                    if (searchcount >= searchResults.size()) {
                        searchcount = 0; 
                    }

                    cursorX = searchResults[searchcount].x;
                    cursorY = searchResults[searchcount].y;
                    searchcount++;
                }
                continue;
            }
            else if (ch == 27) { // ESC
                activeSearch = false;
                searchResults.clear();
                searchcount = 0;
                continue;
            }
            else{
                activeSearch = false;
                searchResults.clear();
                searchcount = 0;
                continue;
            }
        }
        // Check for auto-suggestion trigger after 3 seconds of inactivity
        auto now = std::chrono::system_clock::now();
        std::time_t timeNow = std::chrono::system_clock::to_time_t(now);
        int timeSinceLastEdit = static_cast<int>(timeNow - lastEditTime);
        
        if (timeSinceLastEdit >= AUTO_SUGGESTION_DELAY && !autoSuggestionTriggered && 
            !showInlineSuggestion && !inlineSuggestionExists && allowInlineSuggestion) {
            debugWrite("Auto-triggering inline suggestion after " + std::to_string(timeSinceLastEdit) + " seconds");
            getInlineSuggestion(cursorX, cursorY, buffer, maxInlinePromptSize);
            inlineSuggestionExists = true;
            autoSuggestionTriggered = true;
        }

        ch = getch();
        
        if (ch != ERR) {
            auto now = std::chrono::system_clock::now();
            lastEditTime = std::chrono::system_clock::to_time_t(now);
            autoSuggestionTriggered = false;
            debugWrite("Key pressed: " + std::to_string(ch));
        } else {
            usleep(50000);
            continue;
        }
        int oldXPos = cursorX;
        int oldYPos = cursorY;
        
        // Store buffer state before action for undo/redo
        std::vector<std::string> bufferBeforeAction = buffer;
        
        switch (ch) {
            case CTRL_KEY('q'):
                if (unsavedChanges == true){
                    clear();
                    mvprintw(LINES / 2, 0, "You have unsaved changes. Press 'q' again to quit without saving, or any other key to cancel.");
                    refresh();
                    while(true) {
                        int ch = getch();
                        if (ch == 'q' || ch == 'Q') {
                            endwin();
                            return 0;
                        }
                        else if (ch > 1 && ch != 49 && ch != 81){
                            break;
                        }
                    }
                }
                else{
                    endwin();
                    return 0;
                }
                break;
            case 27: // ESC key
                if (showInlineSuggestion) {
                    showInlineSuggestion = false;
                    inlineBuffer.clear();
                    debugWrite("Inline suggestion cancelled with ESC");
                }
                else if (selectionActive) {
                    selectionActive = false;
                    debugWrite("Selection cancelled with ESC");
                }
                break;
            case 19: // CTRL+S
                debugWrite("CTRL+S pressed - Saving file");
                saveFile(argv[1]);
                break;
            case CTRL_KEY('z'):
                undo(cursorX, cursorY);
                break;
            case CTRL_KEY('y'):
                redo(cursorX, cursorY);
                break;
            case 569:
            debugWrite("CTRL+Tab pressed - Switch to next buffer with active buffer index: " + std::to_string(activeBufferIndex));
                    if (multiFileMode && activeBufferIndex < inactiveBuffer.size() - 1) {
                        changeActiveBuffer(inactiveBuffer,buffer, activeBufferIndex, activeBufferIndex + 1);
                        filename = fileList[activeBufferIndex];
                        //activeBufferIndex++;
                        cursorX = 0;
                        cursorY = 0;
                        break;
                    }
                    else{
                        debugWrite("No next buffer to switch to");
                        break;
                    }
            case 554:
            debugWrite("CTRL+Shift+Tab pressed - Switch to previous buffer with active buffer index: " + std::to_string(activeBufferIndex));
                    if (multiFileMode && activeBufferIndex > 0) {
                        changeActiveBuffer(inactiveBuffer,buffer, activeBufferIndex,activeBufferIndex - 1);
                        filename = fileList[activeBufferIndex];
                        //activeBufferIndex--;
                        cursorX = 0;
                        cursorY = 0;
                        break;
                    }
                    else{
                        debugWrite("No previous buffer to switch to");
                        break;
                    }
            case KEY_F(2):
                lineNumberScheme = (lineNumberScheme == 1) ? 2 : 1;
                contentScheme    = (contentScheme == 3) ? 4 : 3;
                break;
            case 9: {
                if (inlineSuggestionExists == false){
                    debugWrite("Tab pressed - Triggering AI Completion");
                    std::vector<std::string> vectorBeforetxt;
                    vectorBeforetxt.reserve(static_cast<size_t>(cursorY) + 1);
                    int limitLine = std::max(0, cursorY);
                    for (int vecLine = 0; vecLine < limitLine && vecLine < static_cast<int>(buffer.size()); ++vecLine) {
                        vectorBeforetxt.push_back(buffer[vecLine]);
                    }
                    std::string charsBefore;
                    if (cursorY >= 0 && cursorY < static_cast<int>(buffer.size())) {
                        std::size_t bytePos = char_to_byte_index(buffer[cursorY], cursorX);
                        charsBefore = buffer[cursorY].substr(0, bytePos);
                    }
                    vectorBeforetxt.push_back(charsBefore);
                    std::string StrVecTxt;
                    StrVecTxt.reserve(vectorBeforetxt.size() * 8);
                    for (size_t i = 0; i < vectorBeforetxt.size(); ++i) {
                        if (i) StrVecTxt.push_back(',');
                        StrVecTxt += vectorBeforetxt[i];
                    }
                    debugWrite("vector: " + StrVecTxt);
                    std::string promptText = getStingFromVec(vectorBeforetxt);
                    debugWrite("promptText: " + promptText);
                    std::string llamaOutput = AiCompletion(promptText,
                                            (llamaCompletionHost),
                                            llamaCompletionNPredict,
                                            [](const std::string& msg){ debugWrite(msg); }, AiProvider, ollamaModel);
                    debugWrite("got output: " + llamaOutput);
                    for (size_t i = 0; i < llamaOutput.size(); ++i) {
                        char charLlamaOutput = llamaOutput[i];
                        if (charLlamaOutput == '\n') {
                            std::size_t bytePos = char_to_byte_index(buffer[cursorY], cursorX);
                            std::string newLine = buffer[cursorY].substr(bytePos);
                            buffer[cursorY] = buffer[cursorY].substr(0, bytePos);
                            buffer.insert(buffer.begin() + cursorY + 1, newLine);
                            cursorY++;
                            cursorX = 0;
                            if (cursorY >= buffer.size()) {
                                buffer.emplace_back("");
                            }
                            cursorX = 0;
                            continue;
                        }
                        if (charLlamaOutput >= 32 && charLlamaOutput <= 126) {
                            if (cursorY >= buffer.size()) {
                                buffer.emplace_back("");
                            }
                            std::size_t bytePos = char_to_byte_index(buffer[cursorY], cursorX);
                            if (bytePos > buffer[cursorY].size()) {
                                buffer[cursorY].resize(bytePos, ' ');
                            }
                            buffer[cursorY].insert(bytePos, 1, static_cast<char>(charLlamaOutput));
                            cursorX++;
                            unsavedChanges = true;
                        }
                    }
                    break;
                }
                else{
                    // Accept inline Suggestion
                    debugWrite("Accepting inline suggestion");
                    if (!inlineBuffer.empty()) {
                        // Insert inline buffer content into the buffer
                        for (size_t i = 0; i < inlineBuffer.size(); ++i) {
                            const std::string& line = inlineBuffer[i];
                            
                            if (i == 0) {
                                // First line - insert at current cursor position
                                if (cursorY >= buffer.size()) {
                                    buffer.emplace_back("");
                                }
                                std::size_t bytePos = char_to_byte_index(buffer[cursorY], cursorX);
                                if (bytePos > buffer[cursorY].size()) {
                                    buffer[cursorY].resize(bytePos, ' ');
                                }
                                buffer[cursorY].insert(bytePos, line);
                                cursorX += static_cast<int>(line.size()); 
                            } else {
                                
                                std::size_t bytePos = char_to_byte_index(buffer[cursorY], cursorX);
                                std::string restOfLine = buffer[cursorY].substr(bytePos);
                                buffer[cursorY] = buffer[cursorY].substr(0, bytePos);
                                cursorY++;
                                buffer.insert(buffer.begin() + cursorY, line + restOfLine);
                                cursorX = static_cast<int>(line.size()); 
                            }
                        }
                        unsavedChanges = true;
                    }
                    showInlineSuggestion = false;
                    inlineSuggestionExists = false;
                    inlineBuffer.clear();
                    break;
                }
            }
            case 570: {
                if (!selectionActive){
                debugWrite("shift + strg + arrow right");
                selectionActive = true;
                selStartX = cursorX;
                selStartY = cursorY;
                selEndY = cursorY;
                std::string stringAfter = subtractStringLeft(buffer[cursorY], cursorX);
                debugWrite("cursorY pos: " + std::to_string(cursorY));
                debugWrite("current line content:" + buffer[cursorY]);
                std::string onRight = getWordSelectionRight(stringAfter);
                
                debugWrite("OnRight is: " + onRight);
                int moveRight = getUtf8StrLen(onRight);
                cursorX += moveRight;
                selEndX = cursorX;
                break;
                }
                else{
                    debugWrite("shift + strg + arrow right - extending selection");
                    std::string stringAfter = subtractStringLeft(buffer[cursorY], cursorX);
                    debugWrite("cursorY pos: " + std::to_string(cursorY));
                    debugWrite("current line content:" + buffer[cursorY]);
                    std::string onRight = getWordSelectionRight(stringAfter);
                    debugWrite("OnRight is: " + onRight);
                    int moveRight = getUtf8StrLen(onRight);
                    cursorX += moveRight;
                    selEndX = cursorX;
                    debugWrite("selStartX: " + std::to_string(selStartX) + " selEndX: " + std::to_string(selEndX));
                    break;
                }
            }
            case 555:{
                if (!selectionActive){
                debugWrite("shift + strg + arrow left");
                selectionActive = true;
                selStartX = cursorX;
                selStartY = cursorY;
                selEndY = cursorY;
                std::string stringBefore = subtractStringRight(buffer[cursorY], cursorX);
                debugWrite("cursorY pos: " + std::to_string(cursorY));
                debugWrite("current line content:" + buffer[cursorY]);
                std::string onLeft = getWordSelectionLeft(stringBefore);
                debugWrite("OnLeft is: " + onLeft);
                int moveLeft = getUtf8StrLen(onLeft);
                cursorX -= moveLeft;
                if (cursorX < 0) cursorX = 0;
                selEndX = cursorX;
                //switchStartEnd(selStartX, selEndX);
                debugWrite("selStartX: " + std::to_string(selStartX) + " selEndX: " + std::to_string(selEndX));

                break;
                }
                else{
                    debugWrite("shift + strg + arrow left - extending selection");
                    std::string stringBefore = subtractStringRight(buffer[cursorY], cursorX);
                    debugWrite("cursorY pos: " + std::to_string(cursorY));
                    debugWrite("current line content:" + buffer[cursorY]);
                    std::string onLeft = getWordSelectionLeft(stringBefore);
                    debugWrite("OnLeft is: " + onLeft);
                    int moveLeft = getUtf8StrLen(onLeft);
                    cursorX -= moveLeft;
                    if (cursorX < 0) cursorX = 0;
                    selEndX = cursorX;
                    debugWrite("selStartX: " + std::to_string(selStartX) + " selEndX: " + std::to_string(selEndX));
                    break;
                }
            }
            case 337:
                // shift + up
                debugWrite(selectionActive ? "shift + arrow up - extending" : "shift + arrow up");

                if (!selectionActive) {
                    selectionActive = true;
                    selStartX = cursorX;
                    selStartY = cursorY;
                }

                if (cursorY > 0) {
                    cursorY--;
                    if (cursorX > buffer[cursorY].size()) {
                        cursorX = buffer[cursorY].size();
                    }
                } else {
                    cursorX = 0;
                }
                selEndX = cursorX;
                selEndY = cursorY;

                break;
            case 336:
                // shift + down
                debugWrite(selectionActive ? "shift + arrow down - extending" : "shift + arrow down");

                if (!selectionActive) {
                    selectionActive = true;
                    selStartX = cursorX;
                    selStartY = cursorY;
                }
                if (cursorY < buffer.size() - 1) {
                    cursorY++;
                    if (cursorX > buffer[cursorY].size()) {
                        cursorX = buffer[cursorY].size();
                    }
                } else {   
                    cursorX = buffer[cursorY].size();
                }
                selEndX = cursorX;
                selEndY = cursorY;
                break;
            
            case 540:
                //strg + shift + end
                debugWrite(selectionActive ? "shift + end - extending" : "shift + end");
                if (!selectionActive){
                    selectionActive = true;
                    selStartX = cursorX;
                    selStartY = cursorY;
                }
                selEndX = buffer[-1].size();
                selEndY = buffer.size();
                cursorX = buffer[-1].size();
                cursorY = buffer.size();

                break;
            case 545:
                //strg + shift + home
                debugWrite(selectionActive ? "shift + home - extending" : "shift + home");
                if (!selectionActive){
                    selectionActive = true;
                    selStartX = cursorX;
                    selStartY = cursorY;
                }
                selEndX = 0;
                selEndY = 0;
                cursorX = 0;
                cursorY = 0;
                break;
            case 386:
                // shift + end
                debugWrite(selectionActive ? "shift + end - extending" : "shift + end");
                if (!selectionActive){
                    selectionActive = true;
                    selStartX = cursorX;
                    selStartY = cursorY;
                }
                selEndX = buffer[cursorY].size();
                selEndY = cursorY;
                cursorX = buffer[cursorY].size();
                break;
            case 391:
                // shift + home
                debugWrite(selectionActive ? "shift + home - extending" : "shift + home");
                if (!selectionActive){
                    selectionActive = true;
                    selStartX = cursorX;
                    selStartY = cursorY;
                }
                selEndX = 0;
                selEndY = cursorY;
                cursorX = 0;
                break;
            case 402:
                // shift + arrow right
                // adds 1 char to selection and moves cursor
                if (!selectionActive){
                debugWrite("shift + arrow right");
                selectionActive = true;
                selStartX = cursorX;
                selStartY = cursorY;
                selEndY = cursorY;
                if (cursorY < buffer.size()) {
                    std::string stringAfter = subtractStringLeft(buffer[cursorY], cursorX);
                    if (!stringAfter.empty()) {
                        int charLen = getUtf8CharLen(stringAfter, 0);
                        cursorX += charLen;
                        selEndX = cursorX;
                    }
                }
                break;
                }
                else{
                    debugWrite("shift + arrow right - extending selection");
                    if (cursorY < buffer.size()) {
                        std::string stringAfter = subtractStringLeft(buffer[cursorY], cursorX);
                        if (!stringAfter.empty()) {
                            int charLen = getUtf8CharLen(stringAfter, 0);
                            cursorX += charLen;
                            selEndX = cursorX;
                            debugWrite("selStartX: " + std::to_string(selStartX) + " selEndX: " + std::to_string(selEndX));
                        }
                    }
                    break;
                }
            case 393:
                // shift + arrow left
                // adds 1 char to the left to selection and moves cursor
                if (!selectionActive){
                debugWrite("shift + arrow left");
                selectionActive = true;
                selStartX = cursorX;
                selStartY = cursorY;
                selEndY = cursorY;
                if (cursorY < buffer.size()) {
                    std::string stringBefore = subtractStringRight(buffer[cursorY], cursorX);
                    if (!stringBefore.empty()) {
                        int charLen = getUtf8CharLenReverse(stringBefore);
                        cursorX -= charLen;
                        if (cursorX < 0) cursorX = 0;
                        selEndX = cursorX;
                        debugWrite("selStartX: " + std::to_string(selStartX) + " selEndX: " + std::to_string(selEndX));
                    }
                }
                break;
                }
                else{
                    debugWrite("shift + arrow left - extending selection");
                    if (cursorY < buffer.size()) {
                        std::string stringBefore = subtractStringRight(buffer[cursorY], cursorX);
                        if (!stringBefore.empty()) {
                            int charLen = getUtf8CharLenReverse(stringBefore);
                            cursorX -= charLen;
                            if (cursorX < 0) cursorX = 0;
                            selEndX = cursorX;
                            debugWrite("selStartX: " + std::to_string(selStartX) + " selEndX: " + std::to_string(selEndX));
                        }
                    }
                    break;
                }

            case KEY_F(1):
                showHelp();
                break;
            case 544:
                selectionActive = false;
                cursorX = 0;
                cursorY = 0;
                break;
            case KEY_F(7):

                displayAISettings(cursorY, cursorX, rowOffset, argv[1], lineNumberScheme, contentScheme, selectionActive, unsavedChanges, colOffset, authToken, llamaCompletionHost, llamaCompletionNPredict, ollamaModel , AiProvider, inlineSuggestionNPredict, AUTO_SUGGESTION_DELAY);
                cursorX = oldXPos;
                cursorY = oldYPos;
                break;
            case 539:
                selectionActive = false;
                if (!buffer.empty()) {
                    cursorY = static_cast<int>(buffer.size() - 1);
                    cursorX = static_cast<int>(buffer.back().size());
                } else {
                    cursorY = 0;
                    cursorX = 0;
                }
                break;
            case 330:
                if (cursorY >= 0 && cursorY < static_cast<int>(buffer.size())) {
                    std::string &line = buffer[cursorY];
                    int len = static_cast<int>(line.size());
                    if (cursorX > 0 && cursorX <= len) {
                        // Backspace: delete UTF-8 character before cursor
                        size_t charStart = getUtf8CharStart(line, cursorX);
                        int charLen = getUtf8CharLen(line, charStart);
                        line.erase(charStart, charLen);
                        if (cursorX < 0) cursorX = 0;
                    } else if (cursorX >= 0 && cursorX < len) {
                        // Delete: remove UTF-8 character at cursor
                        int charLen = getUtf8CharLen(line, cursorX);
                        line.erase(cursorX, charLen > 0 ? charLen : 1);
                    }
                    unsavedChanges = true;
                }
                break;
            case KEY_F(3):
                if (!selectionActive) {
                    selectionActive = true;
                    selStartY = selEndY = cursorY;
                    selStartX = selEndX = cursorX;
                    debugWrite("Selection started at (" +
                            std::to_string(selStartY) + "," +
                            std::to_string(selStartX) + ")");
                } else {
                    selectionActive = false;
                    debugWrite("Selection ended at (" +
                            std::to_string(selEndY) + "," +
                            std::to_string(selEndX) + ")");
                }
                break;
            case 274:
                inlineSuggestionNPredict++;
            case 273:
                if (inlineSuggestionNPredict > 0){
                    inlineSuggestionNPredict--;
                }
                
            case 1:
                selectionActive = true;
                selStartX = 0;
                selStartY = 0;
                selEndX = buffer[buffer.size()].size();
                selEndY = buffer.size();
                continue;
                break;
            case CTRL_KEY('c'):
                if (selectionActive) {
                    clipboard.clear();
                    copyClipboard(std::min(selStartY, selEndY),
                                std::max(selStartY, selEndY));
                    debugWrite("Copied to clipboard: " + clipboard);
                    selectionActive = false;
                }
                break;
            case CTRL_KEY('v'):
                showInlineSuggestion = false;
                inlineSuggestionExists = false;
                if (!clipboard.empty()) {
                    pasteClipboard(cursorY, cursorX, buffer);
                    debugWrite("Pasted from clipboard at (" +
                            std::to_string(cursorY) + "," +
                            std::to_string(cursorX) + ")");
                }
                break;
            case CTRL_KEY('k'):
                showInlineSuggestion = false;
                inlineSuggestionExists = false;
                if (cursorY < static_cast<int>(buffer.size())) {
                    buffer.erase(buffer.begin() + cursorY);
                    if (cursorY >= static_cast<int>(buffer.size())) cursorY = buffer.size() - 1;
                    if (cursorY < 0) {
                        buffer.push_back("");
                        cursorY = 0;
                    }
                    cursorX = 0;
                    unsavedChanges = true;
                }
                break;
            case KEY_UP:
                if (cursorY > 0) cursorY--;
                cursorX = std::min(cursorX, getUtf8StrLen(buffer[cursorY]));
                showInlineSuggestion = false;
                inlineSuggestionExists = false;
                selectionActive = false;
                break;
            case KEY_DOWN:
                if (cursorY < static_cast<int>(buffer.size()) - 1) cursorY++;
                showInlineSuggestion = false;
                inlineSuggestionExists = false;
                selectionActive = false;
                cursorX = std::min(cursorX, getUtf8StrLen(buffer[cursorY]));
                break;
            case KEY_LEFT:
                if (cursorX > 0) cursorX--;
                showInlineSuggestion = false;
                inlineSuggestionExists = false;
                selectionActive = false;
                break;
            case KEY_RIGHT: {
                int charCount = 0;
                size_t bytePos = 0;
                while (bytePos < buffer[cursorY].size()) {
                    unsigned char c = static_cast<unsigned char>(buffer[cursorY][bytePos]);
                    if ((c & 0x80) == 0) bytePos += 1;
                    else if ((c & 0xE0) == 0xC0) bytePos += 2;
                    else if ((c & 0xF0) == 0xE0) bytePos += 3;
                    else if ((c & 0xF8) == 0xF0) bytePos += 4;
                    else bytePos += 1;
                    charCount++;
                }
                if (cursorX < charCount) cursorX++;
                showInlineSuggestion = false;
                inlineSuggestionExists = false;
                selectionActive = false;
                break;
            }
            case KEY_HOME:
                cursorX = 0;
                showInlineSuggestion = false;
                inlineSuggestionExists = false;
                selectionActive = false;
                break;
            case KEY_END: {
                int charCount = 0;
                size_t bytePos = 0;
                selectionActive = false;
                while (bytePos < buffer[cursorY].size()) {
                    unsigned char c = static_cast<unsigned char>(buffer[cursorY][bytePos]);
                    if ((c & 0x80) == 0) bytePos += 1;
                    else if ((c & 0xE0) == 0xC0) bytePos += 2;
                    else if ((c & 0xF0) == 0xE0) bytePos += 3;
                    else if ((c & 0xF8) == 0xF0) bytePos += 4;
                    else bytePos += 1;
                    charCount++;
                }
                cursorX = charCount;
                showInlineSuggestion = false;
                inlineSuggestionExists = false;
                break;
            }
            case 10: {
                showInlineSuggestion = false;
                inlineSuggestionExists = false;
                std::size_t bytePos = char_to_byte_index(buffer[cursorY], cursorX);
                std::string newLine = buffer[cursorY].substr(bytePos);
                buffer[cursorY] = buffer[cursorY].substr(0, bytePos);
                buffer.insert(buffer.begin() + cursorY + 1, newLine);
                cursorY++;
                cursorX = 0;
                break;
            }
            case KEY_BACKSPACE:
                unsavedChanges = true;
            case 127: {
                showInlineSuggestion = false;
                inlineSuggestionExists = false;
                if (cursorX > 0) {
                    std::size_t bytePos = char_to_byte_index(buffer[cursorY], cursorX);
                    std::size_t prevBytePos = char_to_byte_index(buffer[cursorY], cursorX - 1);
                    buffer[cursorY].erase(prevBytePos, bytePos - prevBytePos);
                    cursorX--;
                } else if (cursorY > 0) {
                    int prevLineCharCount = 0;
                    size_t bytePos = 0;
                    while (bytePos < buffer[cursorY - 1].size()) {
                        unsigned char c = static_cast<unsigned char>(buffer[cursorY - 1][bytePos]);
                        if ((c & 0x80) == 0) bytePos += 1;
                        else if ((c & 0xE0) == 0xC0) bytePos += 2;
                        else if ((c & 0xF0) == 0xE0) bytePos += 3;
                        else if ((c & 0xF8) == 0xF0) bytePos += 4;
                        else bytePos += 1;
                        prevLineCharCount++;
                    }
                    cursorX = prevLineCharCount;
                    buffer[cursorY - 1] += buffer[cursorY];
                    buffer.erase(buffer.begin() + cursorY);
                    cursorY--;
                }
                break;
            }
            case 6:
                searchOverlay(buffer, cursorX, cursorY, activeSearch, searchTerm, SearchLastFoundX, SearchLastFoundY, searchResults);
                debugWrite("got results in Vec: " + posCordsVecToString(searchResults));
                break;
            default: {
                if (ch >= 128 && ch <= 255) {
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
                    if (cursorX > buffer[cursorY].size()) {
                        buffer[cursorY].resize(cursorX, ' ');
                    }
                    std::size_t bytePos = char_to_byte_index(buffer[cursorY], cursorX);
                    buffer[cursorY].insert(bytePos, utf8_char);
                    cursorX += 1;
                    unsavedChanges = true;
                    showInlineSuggestion = false;
                    inlineSuggestionExists = false;
                    break;
                }
                if (ch >= 32 && ch <= 126) {
                    std::size_t bytePos = char_to_byte_index(buffer[cursorY], cursorX);
                    if (bytePos > buffer[cursorY].size()) {
                        buffer[cursorY].resize(bytePos, ' ');
                    }
                    char charToInsert = static_cast<char>(ch);
                    buffer[cursorY].insert(bytePos, 1, charToInsert);
                    cursorX += 1;
                    
                    // Auto-close brackets and quotes
                    if (isOpeningChar(charToInsert)) {
                        char closingChar = getClosingChar(charToInsert);
                        if (closingChar != '\0') {
                            std::size_t closeBytePos = char_to_byte_index(buffer[cursorY], cursorX);
                            buffer[cursorY].insert(closeBytePos, 1, closingChar);
                            // Cursor stays between opening and closing char
                        }
                    }
                    
                    unsavedChanges = true;
                    showInlineSuggestion = false;
                    inlineSuggestionExists = false;
                    break;
                }
                break;
            }
        }

        // Store action in cache if buffer changed or position changed
        if (buffer != bufferBeforeAction || cursorX != oldXPos || cursorY != oldYPos) {
            // Only cache certain actions that change buffer state
            if (ch >= 32 || ch == 10 || ch == KEY_BACKSPACE || ch == 127 || 
                ch == 330 || ch == CTRL_KEY('k') || ch == CTRL_KEY('v') || ch == 9) {
                
                // For paste operations, track the size of pasted content
                int pasteSize = 0;
                if (ch == CTRL_KEY('v')) {
                    pasteSize = clipboard.size();
                }
                
                appendCacheActionBuffer(bufferBeforeAction, buffer, ch, cursorX, cursorY, cacheActionBuffer, maxCacheNum, pasteSize);
            }
        }

        // Update selection end
        if (selectionActive) {
            selEndY = cursorY;
            selEndX = cursorX;
        }
    }

    endwin();
    return 0;
}