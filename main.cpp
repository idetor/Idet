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
#include "headers/light/bash.hpp"
#include <sys/stat.h>
//#include "headers/LlamaClient.hpp"
std::ofstream debugOut;

void debugWrite(const std::string& msg) {
    if (debugOut.is_open()) {
        debugOut << msg << std::endl;
        debugOut.flush();
    }
}


#include "headers/networkAIApi.hpp"
#include "headers/editorFunctions.h"


const std::string version = "0.1.5-alpha";


#define CTRL_KEY(k) ((k) & 0x1f)

// init Vars

// General Vars

//int lastModifiedTime = 0;
//std::vector<std::string> buffer;
//std::vector<std::string> initialFileBuffer;
//std::vector<std::string> inlineBuffer;
//int inlineBufferPosX = 0;
//int inlineBufferPosY = 0;

SelectionElements selection;
bool selectionActive = false;
int selStartY = 0, selStartX = 0;
int selEndY = 0, selEndX = 0;

std::string clipboard;
//bool unsavedChanges = false;
//bool createNewFile = true;
std::string configPath = expandPath("~/.config/idet/config");
//int lastEditTime = 0;
//std::string filename;
std::vector<cacheAction> cacheActionBuffer; 
int cacheIndex = -1; 
int savedCacheIndex = -1;
std::vector<std::vector<std::string>> inactiveBuffer;
std::vector<std::string> fileList;
int activeBufferIndex = 0;
std::vector<char> openCharList;
bool activeSearch = false;
std::string searchTerm = "";
int SearchLastFoundX = -1;
int SearchLastFoundY = -1;
std::vector<posCords> searchResults;
int searchcount = 0;
std::string detectedLang = "";
std::vector<fileElements> fileElementsBuffer;
bool tabOverlayActive = false;
tabOverlayParams tabParams;
bool inplacementHappened = false;

// Configurable
int lineNumberScheme = 1; // 1 or 2
int contentScheme = 3;    // 3 or 4
const size_t DEBUG_MAX = 10000;
int maxCacheNum = 100;
bool multiFileMode = false;
int tabSpaces = 4;

// AI Vars

AiProps AiSettings;

//std::string modelPath = "/var/models/tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf";
//std::string authToken = "";
//std::string llamaCompletionHost = "http://localhost:8080"; //URL of llamacpp
//std::string llamaCompletionNPredict = "5"; // how many tokens to generate with TAB
//std::string ollamaModel = "gpt-oss:20b";
//std::string AiProvider = "llamacpp";
//int AiSettings.inlineSuggestionNPredict = 5;
//int AiSettings.AUTO_SUGGESTION_DELAY = 3;
//int maxInlinePromptSize = 10000;
bool llamaInit = false;
bool modelLoaded = false;
bool showInlineSuggestion = false;
bool inlineSuggestionExists = false;
bool allowInlineSuggestion = true;
bool autoSuggestionTriggered = false;


void debugWrite(std::ofstream& out, const std::string& msg) {
    if (!out.is_open()) return;

    if (msg.size() <= DEBUG_MAX) {
        out << msg;
    } else {
        out << msg.substr(0, DEBUG_MAX);
    }
}


int main(int argc, char* argv[]) {
    // Set locale for UTF-8 support
    setlocale(LC_ALL, "");
    std::locale::global(std::locale(""));
    
    File file;

    if (argc <= 1){
        std::cerr << "No file or parameter given!\n";
        return 1;
    }
    std::string debugTTY;
    // check the early args
    for (int i = 1; i < argc; i++) {
        if (tolowerString(std::string(argv[i])) == "--multifile") {
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
            if (i + 1 < argc) AiSettings.AiProvider = argv[++i];
        }
        else if (arg == "-m" || arg == "--model") {
            if (i + 1 < argc) AiSettings.modelPath = argv[++i];
        }
        else if (arg == "-a" || arg == "--auth") {
            if (i + 1 < argc) AiSettings.authToken = argv[++i];
        }
        else if (arg == "--ollamaModel") {
            if (i + 1 < argc) AiSettings.ollamaModel = argv[++i];
        }
        else if (arg == "-i" || arg == "--inline") {
            if (i + 1 < argc) {
                try {
                    AiSettings.inlineSuggestionNPredict = std::stoi(argv[++i]);
                } catch (...) {}
            }
        }
        else if (arg == "-h" || arg == "--host") {
            if (i + 1 < argc) {
                std::string val = argv[++i];
                if (val.rfind("http", 0) == 0)
                    AiSettings.llamaCompletionHost = val;
                else
                    AiSettings.llamaCompletionHost = "http://" + val;
            }
        }
        else if (arg == "-n" || arg == "--npredict") {
            if (i + 1 < argc) AiSettings.llamaCompletionNPredict = argv[++i];
        }
        else if (arg == "--noNewFile") {
            file.createNewFile = false;
        }
        else if (arg[0] != '-') {
            
            if (multiFileMode) {
                fileList.push_back(arg);
                //std::cerr << "Adding file: " << arg << "\n";
            } else {
                file.filename = arg;
            }
        }
    }

    // multiline Mode not implemented yet!!!
    /*
    if (multiFileMode){
        if (fileList.empty()){
            std::cerr << "Multi-file mode enabled but no files provided!\n";
            return 1;
        }
        debugWrite("Files to load: " + strVecToString(fileList));
        filename = fileList[0]; // set first file as main buffer file
        if(multiFileMode == true){
            loadInfileElements(fileElementsBuffer, filename);
        }
        
    }
    */
    if (isDirectory(file.filename)){
        std::cerr << "Provided Filename is a Directory and can not be opened: " + file.filename + "\n";
        return 1;
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
            loadConfig(configPath, AiSettings);
    }
    else {
        debugWrite("!!!No config file found at " + configPath);
    }
    debugWrite("Loading File: " + file.filename);
    file.load();
    /*
    if (multiFileMode == true) {
        
        inactiveBuffer.push_back(buffer);
        buffer.clear();
        
        
        for (int i = 1; i < fileList.size(); i++) {
            std::string handlingFile = fileList[i];
            if (isDirectory(handlingFile)){
                std::cerr << "Provided Filename is a Directory and can not be opened: " + handlingFile + "\n";
                return 1;
            }
            std::vector<std::string> tmpFileBuffer;
            loadFile(handlingFile, tmpFileBuffer, initialFileBuffer, lastModifiedTime);
            inactiveBuffer.push_back(tmpFileBuffer);
        }
        
        
        for (int i = 0; i < fileList.size(); i++) {
            if (i < (int)fileElementsBuffer.size()) {
                
                continue;
            }
            loadInfileElements(fileElementsBuffer, fileList[i]);
        }
        
        
        if (!inactiveBuffer.empty()) {
            buffer = std::move(inactiveBuffer[0]);
            inactiveBuffer[0].clear();
            activeBufferIndex = 0;
            debugWrite("Multi-file mode: loaded " + std::to_string(fileList.size()) + " files");
        }
    }
    */
    // detect lang
    //detectLanguage(buffer, detectedLang, file.filename);


    // Initialize ncurses
    initscr();
    start_color();
    use_default_colors();
    init_pair(1, COLOR_BLACK, COLOR_CYAN);   // line numbers default
    init_pair(2, COLOR_WHITE, COLOR_BLUE);   // alternate line numbers
    init_pair(3, COLOR_WHITE, COLOR_BLACK);  // content default
    init_pair(4, COLOR_YELLOW, COLOR_BLACK); // alternate content
    init_pair(5, COLOR_CYAN, COLOR_BLUE); // suggestive content

    init_pair(10, COLOR_CYAN, COLOR_BLUE);
    init_pair(11, COLOR_BLUE, COLOR_BLACK);  // bash commands - green
    init_pair(12, COLOR_CYAN, COLOR_BLACK);   // bash keywords - cyan
    init_pair(13, COLOR_YELLOW, COLOR_BLACK); // script definitions - yellow
    init_pair(14, COLOR_MAGENTA, COLOR_BLACK); // operators - magenta
    init_pair(15, COLOR_GREEN, COLOR_BLACK); // comments - white (will be rendered as comments)
    init_pair(100, COLOR_WHITE,COLOR_BLACK); 
    init_pair(110, COLOR_BLACK,COLOR_WHITE);
    raw();
    keypad(stdscr, TRUE);
    noecho();
    
    // Load built-in commands immediately for instant syntax highlighting
    initializeBashCommandsBuiltInOnly();
    
    // Start background thread to load all system commands (expensive on slow systems)
    std::thread commandLoader([]{ loadAllCommandsAsync(); });
    commandLoader.detach();
    nodelay(stdscr, TRUE);

    int cursorX = 0, cursorY = 0;
    int rowOffset = 0;
    int colOffset = 0;
    int ch;
    

    auto initTime = std::chrono::system_clock::now();
    //lastEditTime = std::chrono::system_clock::to_time_t(initTime);
    /*
    if(multiFileMode){
        SetInfileElements(fileElementsBuffer, activeBufferIndex, lastModifiedTime, unsavedChanges, selStartX, selStartY, selEndX, selEndY, cursorX, cursorY);
    }
    */
    while (true) {
        inplacementHappened = false;
        int maxVisibleRows = LINES - 2;
        if (cursorY - rowOffset >= maxVisibleRows) rowOffset = cursorY - maxVisibleRows + 1;
        if (cursorY - rowOffset < 0) rowOffset = cursorY;
        /*
        if (tabOverlayActive && tabParams.exists) {
            tabParams.buffer = buffer;
            tabParams.cursorX = cursorX;
            tabParams.cursorY = cursorY;
            debugWrite("Tab overlay parameters updated - cursor: (" + std::to_string(cursorX) + ", " + std::to_string(cursorY) + ")");
        }
        */
        std::vector<std::string> buffer = file.buffer;
        draw(cursorY, cursorX, rowOffset,
            file.filename, lineNumberScheme, contentScheme,
            selectionActive, file.unsavedChanges, colOffset,
            AiSettings.inlineSuggestionNPredict, multiFileMode, fileList,
            activeBufferIndex, detectedLang, file.buffer,
            selStartX, selStartY, selEndX,
            selEndY, showInlineSuggestion, file.lastModifiedTime,
            tabOverlayActive, tabParams, file.inlineBuffer,
            file.inlineBufferPosX, file.inlineBufferPosY);
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
        auto now = std::chrono::system_clock::now();
        std::time_t timeNow = std::chrono::system_clock::to_time_t(now);
        //int timeSinceLastEdit = static_cast<int>(timeNow - lastEditTime);
        /*
        if (timeSinceLastEdit >= AiSettings.AUTO_SUGGESTION_DELAY && !autoSuggestionTriggered && 
            !showInlineSuggestion && !inlineSuggestionExists && allowInlineSuggestion) {
            debugWrite("Auto-triggering inline suggestion after " + std::to_string(timeSinceLastEdit) + " seconds");
            getInlineSuggestion(cursorX, cursorY, buffer, AiSettings.maxInlinePromptSize, AiSettings, inlineBuffer, inlineBufferPosX, inlineBufferPosY, showInlineSuggestion);
            inlineSuggestionExists = true;
            autoSuggestionTriggered = true;
            detectLanguage(buffer, detectedLang, filename); 
        }
        */
        ch = getch();
        
        if (ch != ERR) {
            auto now = std::chrono::system_clock::now();
            //lastEditTime = std::chrono::system_clock::to_time_t(now);
            autoSuggestionTriggered = false;
            debugWrite("Key pressed: " + std::to_string(ch));
            

            if (ch != KEY_RIGHT && ch != KEY_LEFT && ch != KEY_UP && ch != KEY_DOWN && 
                ch != KEY_HOME && ch != KEY_END && ch != 402 && ch != 393) {
                tabOverlayActive = false;
                tabParams.exists = false;
                debugWrite("Tab overlay disabled");
            }
        } else {
            usleep(50000);
            continue;
        }
        int oldXPos = cursorX;
        int oldYPos = cursorY;
        

        std::vector<std::string> bufferBeforeAction = buffer;
        
        switch (ch) {
            case 4:{
                if (!buffer.empty() && cursorY >= 0 && cursorY < static_cast<int>(buffer.size())) {
                    tabParams.buffer = buffer;
                    tabParams.cursorX = cursorX;
                    tabParams.cursorY = cursorY;
                    tabParams.exists = true;
                    tabParams.needsUpdate = true;  
                    tabOverlayActive = true;
                    debugWrite("Tab overlay activated");
                } else {
                    debugWrite("Tab overlay cannot activate - invalid buffer state");
                }
                break;
            }

            case CTRL_KEY('q'):
                if (file.unsavedChanges == true){
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
                    file.inlineBuffer.clear();
                    debugWrite("Inline suggestion cancelled with ESC");
                }
                else if (selectionActive) {
                    selectionActive = false;
                    debugWrite("Selection cancelled with ESC");
                }
                break;
            case 19: // CTRL+S
                debugWrite("CTRL+S pressed - Saving file");
                file.save();
                break;
            case CTRL_KEY('z'):
                undo(cursorX, cursorY, buffer, cacheActionBuffer, cacheIndex, savedCacheIndex, file.initialFileBuffer, file.unsavedChanges);
                break;
            case CTRL_KEY('y'):
                redo(cursorX, cursorY, buffer, cacheActionBuffer, cacheIndex, savedCacheIndex, file.initialFileBuffer, file.unsavedChanges);
                break;
            case 569:
                debugWrite("CTRL+Tab pressed - Switch to next buffer with active buffer index: " + std::to_string(activeBufferIndex));
                    if (multiFileMode && activeBufferIndex < inactiveBuffer.size() - 1) {
                        changeFileElements(fileElementsBuffer,activeBufferIndex,activeBufferIndex + 1,file.lastModifiedTime, file.unsavedChanges,selStartX,selStartY,selEndX,selEndY, cursorX, cursorY);
                        changeActiveBuffer(inactiveBuffer,buffer, activeBufferIndex, activeBufferIndex + 1);
                        file.filename = fileList[activeBufferIndex];
                        //activeBufferIndex++;

                        break;
                    }
                    else{
                        debugWrite("No next buffer to switch to");
                        break;
                    }
            case 291:
                debugWrite("CTRL+Tab pressed - Switch to next buffer with active buffer index: " + std::to_string(activeBufferIndex));
                    if (multiFileMode && activeBufferIndex < inactiveBuffer.size() - 1) {
                        changeFileElements(fileElementsBuffer,activeBufferIndex,activeBufferIndex + 1, file.lastModifiedTime,file.unsavedChanges,selStartX,selStartY,selEndX,selEndY, cursorX, cursorY);
                        changeActiveBuffer(inactiveBuffer,buffer, activeBufferIndex, activeBufferIndex + 1);
                        file.filename = fileList[activeBufferIndex];
                        //activeBufferIndex++;

                        break;
                    }
                    else{
                        debugWrite("No next buffer to switch to");
                        break;
                    }
            case 554:
                debugWrite("CTRL+Shift+Tab pressed - Switch to previous buffer with active buffer index: " + std::to_string(activeBufferIndex));
                    if (multiFileMode && activeBufferIndex > 0) {
                        changeFileElements(fileElementsBuffer,activeBufferIndex,activeBufferIndex - 1,file.lastModifiedTime, file.unsavedChanges,selStartX,selStartY,selEndX,selEndY, cursorX, cursorY);
                        changeActiveBuffer(inactiveBuffer,buffer, activeBufferIndex,activeBufferIndex - 1);
                        file.filename = fileList[activeBufferIndex];
                        //activeBufferIndex--;

                        break;
                    }
                    else{
                        debugWrite("No previous buffer to switch to");
                        break;
                    }
            case 290:
                debugWrite("CTRL+Shift+Tab pressed - Switch to previous buffer with active buffer index: " + std::to_string(activeBufferIndex));
                    if (multiFileMode && activeBufferIndex > 0) {
                        changeFileElements(fileElementsBuffer,activeBufferIndex,activeBufferIndex - 1, file.lastModifiedTime, file.unsavedChanges,selStartX,selStartY,selEndX,selEndY, cursorX, cursorY);
                        changeActiveBuffer(inactiveBuffer,buffer, activeBufferIndex,activeBufferIndex - 1);
                        file.filename = fileList[activeBufferIndex];
                        //activeBufferIndex--;

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
            case 9:
                // TAB accepts inline suggestion or inserts spaces
                {
                    if (inlineSuggestionExists && !file.inlineBuffer.empty()) {
                        // Accept inline suggestion with Tab
                        debugWrite("Accepting inline suggestion via Tab");
                        for (size_t i = 0; i < file.inlineBuffer.size(); ++i) {
                            const std::string& line = file.inlineBuffer[i];
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
                                // Subsequent lines
                                std::size_t bytePos = char_to_byte_index(buffer[cursorY], cursorX);
                                std::string restOfLine = buffer[cursorY].substr(bytePos);
                                buffer[cursorY] = buffer[cursorY].substr(0, bytePos);
                                cursorY++;
                                buffer.insert(buffer.begin() + cursorY, line + restOfLine);
                                cursorX = static_cast<int>(line.size());
                            }
                        }
                        file.unsavedChanges = true;
                        showInlineSuggestion = false;
                        inlineSuggestionExists = false;
                        file.inlineBuffer.clear();
                    } else {
                        // No suggestion - insert tab spaces
                        std::size_t bytePos = char_to_byte_index(buffer[cursorY], cursorX);
                        buffer[cursorY].insert(bytePos, tabSpaces, ' ');
                        cursorX += tabSpaces;
                        file.unsavedChanges = true;
                    }
                }
                break;
            case 0: {
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
                                            (AiSettings.llamaCompletionHost),
                                            AiSettings.llamaCompletionNPredict,
                                            [](const std::string& msg){ debugWrite(msg); }, AiSettings.AiProvider, AiSettings.ollamaModel);
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
                            file.unsavedChanges = true;
                        }
                    }
                    break;
                }
                else{
                    // Accept inline Suggestion
                    debugWrite("Accepting inline suggestion");
                    if (!file.inlineBuffer.empty()) {
                        // Insert inline buffer content into the buffer
                        for (size_t i = 0; i < file.inlineBuffer.size(); ++i) {
                            const std::string& line = file.inlineBuffer[i];
                            
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
                        file.unsavedChanges = true;
                    }
                    showInlineSuggestion = false;
                    inlineSuggestionExists = false;
                    file.inlineBuffer.clear();
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
                showHelp(version, lineNumberScheme);
                break;
            case 544:
                selectionActive = false;
                cursorX = 0;
                cursorY = 0;
                break;
            case KEY_F(7):

                displayAISettings(cursorY, cursorX, rowOffset, argv[1], lineNumberScheme, contentScheme, selectionActive, file.unsavedChanges, colOffset, AiSettings);
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
                if (selectionActive) {
                    // Delete selected content
                    int startX = selStartX;
                    int startY = selStartY;
                    int endX   = selEndX;
                    int endY   = selEndY;

                    if (startY > endY || (startY == endY && startX > endX)) {
                        std::swap(startX, endX);
                        std::swap(startY, endY);
                    }

                    if (startY == endY) {
                        buffer[startY].erase(startX, endX - startX);
                    }
                    else {
                        buffer[startY].erase(startX);
                        buffer[endY].erase(0, endX);
                        buffer[startY] += buffer[endY];
                        buffer.erase(buffer.begin() + startY + 1,
                                    buffer.begin() + endY + 1);
                    }
                    cursorX = startX;
                    cursorY = startY;
                    selectionActive = false;
                    file.unsavedChanges = true;
                } else if (cursorY >= 0 && cursorY < static_cast<int>(buffer.size())) {
                    std::string &line = buffer[cursorY];
                    int charCount = getUtf8StrLen(line);
                    if (cursorX >= 0 && cursorX < charCount) {
                        // Delete: remove UTF-8 character at cursor
                        std::size_t bytePos = char_to_byte_index(line, cursorX);
                        int charLen = getUtf8CharLen(line, bytePos);
                        debugWrite("Char len: " + std::to_string(charLen));
                        line.erase(bytePos, charLen > 0 ? charLen : 1);
                    }
                    file.unsavedChanges = true;
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
            case 18:
                debugWrite("Reloading File");
                erase();
                //reloadFile(filename,buffer, initialFileBuffer, lastModifiedTime, cacheActionBuffer, cacheIndex, savedCacheIndex);

                
                continue;
            case 274:
                AiSettings.inlineSuggestionNPredict++;
                break;
            case 273:
                if (AiSettings.inlineSuggestionNPredict > 0){
                    AiSettings.inlineSuggestionNPredict--;
                }
                break;
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
                                std::max(selStartY, selEndY), selStartX, selStartY, selEndX, selEndY, buffer, clipboard);
                    debugWrite("Copied to clipboard: " + clipboard);
                    selectionActive = false;
                }
                break;
            case CTRL_KEY('v'):
                showInlineSuggestion = false;
                inlineSuggestionExists = false;
                if (!clipboard.empty()) {
                    pasteClipboard(cursorY, cursorX, buffer, clipboard);
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
                    file.unsavedChanges = true;
                }
                break;
            case 272:
                debugWrite(strVecToString(customCommandsBuiltIn));
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
                int charCount = getUtf8StrLen(buffer[cursorY]);
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
                selectionActive = false;
                cursorX = getUtf8StrLen(buffer[cursorY]);
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
                file.unsavedChanges = true;
            case 127: {
                showInlineSuggestion = false;
                inlineSuggestionExists = false;
                file.unsavedChanges = true;
                if (selectionActive) {
                    // Delete selected content
                    int startX = selStartX;
                    int startY = selStartY;
                    int endX   = selEndX;
                    int endY   = selEndY;

                    if (startY > endY || (startY == endY && startX > endX)) {
                        std::swap(startX, endX);
                        std::swap(startY, endY);
                    }

                    if (startY == endY) {
                        buffer[startY].erase(startX, endX - startX);
                    }
                    else {
                        buffer[startY].erase(startX);
                        buffer[endY].erase(0, endX);
                        buffer[startY] += buffer[endY];
                        buffer.erase(buffer.begin() + startY + 1,
                                    buffer.begin() + endY + 1);
                    }
                    cursorX = startX;
                    cursorY = startY;
                    selectionActive = false;
                } else if (cursorX > 0) {
                    
                    int spacesBeforeCursor = NdirectspacesBeforeNum(buffer[cursorY], cursorX);
                    int deleteCount = 1; 
                    
                    
                    if (spacesBeforeCursor > 0 && spacesBeforeCursor % tabSpaces == 0) {
                        deleteCount = tabSpaces;
                    }
                    
                    
                    for (int i = 0; i < deleteCount && cursorX > 0; i++) {
                        std::size_t bytePos = char_to_byte_index(buffer[cursorY], cursorX);
                        std::size_t prevBytePos = char_to_byte_index(buffer[cursorY], cursorX - 1);
                        buffer[cursorY].erase(prevBytePos, bytePos - prevBytePos);
                        cursorX--;
                    }
                } else if (cursorY > 0) {
                    cursorX = getUtf8StrLen(buffer[cursorY - 1]);
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
                // Handle selection deletion first if selection is active
                if(selectionActive){
                    int startX = selStartX;
                    int startY = selStartY;
                    int endX   = selEndX;
                    int endY   = selEndY;

                    if (startY > endY || (startY == endY && startX > endX)) {
                        std::swap(startX, endX);
                        std::swap(startY, endY);
                    }

                    if (startY == endY) {
                        buffer[startY].erase(startX, endX - startX);
                    }
                    else {
                        buffer[startY].erase(startX);
                        buffer[endY].erase(0, endX);
                        buffer[startY] += buffer[endY];
                        buffer.erase(buffer.begin() + startY + 1,
                                    buffer.begin() + endY + 1);
                    }
                    cursorX = startX;
                    cursorY = startY;
                    selectionActive = false;
                    inplacementHappened = true;
                    debugWrite("Inplacement Happened" + std::to_string(inplacementHappened));
                }
                
                // Now handle character insertion
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
                    file.unsavedChanges = true;
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
                    
                    file.unsavedChanges = true;
                    showInlineSuggestion = false;
                    inlineSuggestionExists = false;
                    break;
                }
                break;

            }
        }

        // Store action in cache if buffer changed or position changed
        if (buffer != bufferBeforeAction || cursorX != oldXPos || cursorY != oldYPos || inplacementHappened) {
            
            // Only cache certain actions that change buffer state
            if (ch >= 32 || ch == 10 || ch == KEY_BACKSPACE || ch == 127 || 
                ch == 330 || ch == CTRL_KEY('k') || ch == CTRL_KEY('v') || ch == 9 || (ch < 32 && selectionActive || inplacementHappened)) {
                
                // For paste operations, track the size of pasted content
                int pasteSize = 0;
                if (ch == CTRL_KEY('v')) {
                    pasteSize = clipboard.size();
                }
                debugWrite("Appending CacheActionBuffer");
                appendCacheActionBuffer(bufferBeforeAction, buffer, ch, cursorX, cursorY, cacheActionBuffer, maxCacheNum, cacheIndex, pasteSize);
            }
        }

        // Update selection end
        if (selectionActive) {
            selEndY = cursorY;
            selEndX = cursorX;
        }
        file.buffer = buffer;
    }
    endwin();
    return 0;
}