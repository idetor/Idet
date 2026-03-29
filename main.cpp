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

// AI Vars
std::string modelPath = "/var/models/tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf";
std::string authToken = "";
std::string llamaCompletionHost = "http://localhost:8080"; //URL of llamacpp
std::string llamaCompletionNPredict = "5"; // how many tokens to generate with TAB
std::string ollamaModel = "gpt-oss:20b";
std::string AiProvider = "llamacpp";
int inlineSuggestionNPredict = 5;
int AUTO_SUGGESTION_DELAY = 3;
bool llamaInit = false;
bool modelLoaded = false;
bool showInlineSuggestion = false;
bool inlineSuggestionExists = false;
bool allowInlineSuggestion = true;
bool autoSuggestionTriggered = false;








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

void loadFile(const std::string& filename) {
    if (!checkFileExistance(filename)) {
        debugWrite("file does not exist");
        buffer.clear();
        buffer.emplace_back();
        lastModifiedTime = 0;
        return;
    }

    std::ifstream file(filename);
    if (!file) {
        buffer.clear();
        lastModifiedTime = 0;
        return;
    }

    buffer.clear();
    std::string line;
    while (std::getline(file, line)) {
        buffer.push_back(line);
    }
    if (buffer.empty()) buffer.push_back("");

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
                int lineStartX = (y == selStartY) ? selStartX : 0;
                int lineEndX   = (y == selEndY) ? selEndX : buffer[y].size();
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

std::string getWordSelectionRight(const std::string rightString) {
    std::string wordRight = "";

    for (char c : rightString) {
        if (c == ' ') {
            return wordRight; 
        }
        wordRight += c;
    }

    return wordRight; 
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

std::string subtractStringLeft(const std::string fullString, int subtraction) {
    if (subtraction <= 0) {
        return fullString; // nothing to remove
    }

    if (subtraction >= fullString.length()) {
        return ""; // remove everything
    }

    return fullString.substr(subtraction);
}

void draw(int cursorY, int cursorX, int& rowOffset, const std::string& filename,int lineNumberScheme, int contentScheme, bool selectionActive,bool unsavedChanges, int& colOffset, int inlineSuggestionNPredict = 0) 
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
mvprintw(0, 0, "Idet-Editor - File: %s%s | Selection: %s | Suggestion Length: %d",
         filename.c_str(),
         unsavedChanges ? "*" : "",
         selectionActive ? "ON" : "OFF", 
        inlineSuggestionNPredict
        );
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

    // Convert UTF-8 → wide string using safe conversion
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
mvprintw(LINES - 1, 0, "CTRL+S=Save | CTRL+Q=Quit | Line %d/%d | Column %d/%d | Last Modified: %s",
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

void drawAISettings(){

}

void displayAISettings(){
    while (true)
    {
        drawAISettings();
        int settingsCh = getch();
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

void getInlineSuggestion(int cursorX, int cursorY){
        debugWrite("Tab pressed - Triggering AI Completion");
        std::vector<std::string> vectorBeforetxt;
        vectorBeforetxt.reserve(static_cast<size_t>(cursorY) + 1); // avoid reallocs

        int limitLine = std::max(0, cursorY); // ensure non-negative
        for (int vecLine = 0; vecLine < limitLine && vecLine < static_cast<int>(buffer.size()); ++vecLine) {
            vectorBeforetxt.push_back(buffer[vecLine]);
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


int main(int argc, char* argv[]) {
    // Set locale for UTF-8 support
    setlocale(LC_ALL, "");
    std::locale::global(std::locale(""));
    
    if (argc <= 1){
        std::cerr << "No file or parameter given!\n";
        return 1;
    }
    std::string debugTTY;
    // check the args
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "-d" && i + 1 < argc) {
            debugTTY = argv[i + 1];
        }
        if (std::string(argv[i]) == "-h" || std::string(argv[i]) == "--help") {
            printf("Usage: %s <filename> [-d debug_pipe]\n", argv[0]);
            return 0;
        }
        if (std::string(argv[i]) == "-v" || std::string(argv[i]) == "--version") {
            printf("Version %s\n", version.c_str());
            return 0;
        }
        if (std::string(argv[i]) == "-p" || std::string(argv[i]) == "--provider") {
            
            AiProvider = argv[i + 1];
        }
        if (std::string(argv[i]) == "-m" || std::string(argv[i]) == "--model") {
            if (argv[i + 1]){
                modelPath = argv[i + 1];
            }
        }
        if (std::string(argv[i]) == "-a" || std::string(argv[i]) == "--auth") {
            if (argv[i + 1]){
                authToken = argv[i + 1]; 
            }
        }
        if (std::string(argv[i]) == "--ollamaModel") {
            if (argv[i + 1]){
                ollamaModel = argv[i+1];
            }
        }
        if (i + 1 < argc &&
            (std::string_view(argv[i]) == "-i" || std::string_view(argv[i]) == "--inline"))
        {
            // Convert the next argument to an integer
            try {
                inlineSuggestionNPredict = std::stoi(argv[i + 1]);  // throws if not numeric
            } catch (const std::invalid_argument&) {
                // handle “not a number” error
            } catch (const std::out_of_range&) {
                // handle “too large” error
            }
        }
        if (std::string(argv[i]) == "-h" || std::string(argv[i]) == "--host") {
            if (argv[i+1][0] == 'h' || argv[i+1][1] == 't'){
                llamaCompletionHost = argv[i + 1];
            }
            else{
                llamaCompletionHost = "http://";
                llamaCompletionHost.append(argv[i + 1]);
            }
            debugWrite("using llamaCompletionHost: " + llamaCompletionHost);
        }
        if (std::string(argv[i]) == "-n" || std::string(argv[i]) == "--npredict") {
            llamaCompletionNPredict = argv[i + 1];
            
        }
        if (std::string(argv[i]) == "--noNewFile") {
            createNewFile = false;
        }
        //if(!std::string(argv[i]).find('-', 0) && std::string(argv[i-1]).find('-', 0)){
        //    filename = std::string(argv[i]);
        //}
        if (argv[i - 1][0] != '-' && argv[i][0] != '-'){
            
            filename = argv[i]; 
        }
    }
    if (!debugTTY.empty()) {
    debugOut.open(debugTTY);
    if (!debugOut.is_open()) {
        fprintf(stderr, "Failed to open debug pipe: %s\n", debugTTY.c_str());
    }
    }
    if (checkFileExistance(configPath) == true){
            ConfigLoader config(configPath);
    }
    else {
        debugWrite("No config file found");
    }
    debugWrite("Editor started");

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
    
    // Initialize ncurses
    initscr();
    start_color();
    use_default_colors();
    init_pair(1, COLOR_BLACK, COLOR_CYAN);   // line numbers default
    init_pair(2, COLOR_WHITE, COLOR_BLUE);   // alternate line numbers
    init_pair(3, COLOR_WHITE, COLOR_BLACK);  // content default
    init_pair(4, COLOR_YELLOW, COLOR_BLACK); // alternate content

    init_pair(10, COLOR_CYAN, COLOR_BLUE);

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

        draw(cursorY, cursorX, rowOffset, argv[1], lineNumberScheme, contentScheme, selectionActive, unsavedChanges, colOffset, inlineSuggestionNPredict);

        // Check for auto-suggestion trigger after 3 seconds of inactivity
        auto now = std::chrono::system_clock::now();
        std::time_t timeNow = std::chrono::system_clock::to_time_t(now);
        int timeSinceLastEdit = static_cast<int>(timeNow - lastEditTime);
        
        if (timeSinceLastEdit >= AUTO_SUGGESTION_DELAY && !autoSuggestionTriggered && 
            !showInlineSuggestion && !inlineSuggestionExists && allowInlineSuggestion) {
            debugWrite("Auto-triggering inline suggestion after " + std::to_string(timeSinceLastEdit) + " seconds");
            getInlineSuggestion(cursorX, cursorY);
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

        switch (ch) {
            case CTRL_KEY('q'):
                endwin();
                exit(0);
            case CTRL_KEY('s'):
                saveFile(argv[1]);
                break;
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
                                cursorX += static_cast<int>(line.size()); // size() is enough since we know it's ASCII now
                            } else {
                                // Subsequent lines - create new lines
                                std::size_t bytePos = char_to_byte_index(buffer[cursorY], cursorX);
                                std::string restOfLine = buffer[cursorY].substr(bytePos);
                                buffer[cursorY] = buffer[cursorY].substr(0, bytePos);
                                cursorY++;
                                buffer.insert(buffer.begin() + cursorY, line + restOfLine);
                                cursorX = static_cast<int>(line.size()); // size() is enough since we know it's ASCII now
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
            case 402: {
                debugWrite("shift + arrow right");
                std::string stringAfter = subtractStringLeft(buffer[cursorY], cursorX);
                debugWrite("cursorY pos: " + std::to_string(cursorY));
                debugWrite("current line content:" + buffer[cursorY]);
                std::string onRight = getWordSelectionRight(stringAfter);
                debugWrite("OnRight is: " + onRight);
                break;
            }
            case KEY_F(1):
                showHelp();
                break;
            case 544:
                cursorX = 0;
                cursorY = 0;
                break;
            case KEY_F(7):

                getInlineSuggestion(cursorX, cursorY); 
                inlineSuggestionExists = true;
                break;
            case 539:
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
                        line.erase(cursorX - 1, 1);
                        --cursorX;
                    } else if (cursorX >= 0 && cursorX < len) {
                        line.erase(cursorX, 1);
                    }
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
                showInlineSuggestion = false;
                inlineSuggestionExists = false;
                break;
            case KEY_DOWN:
                if (cursorY < static_cast<int>(buffer.size()) - 1) cursorY++;
                showInlineSuggestion = false;
                inlineSuggestionExists = false;
                break;
            case KEY_LEFT:
                if (cursorX > 0) cursorX--;
                showInlineSuggestion = false;
                inlineSuggestionExists = false;
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
                break;
            }
            case KEY_HOME:
                cursorX = 0;
                showInlineSuggestion = false;
                inlineSuggestionExists = false;
                break;
            case KEY_END: {
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
                    buffer[cursorY].insert(bytePos, 1, static_cast<char>(ch));
                    cursorX += 1;
                    unsavedChanges = true;
                    showInlineSuggestion = false;
                    inlineSuggestionExists = false;
                    break;
                }
                break;
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