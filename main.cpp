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

SelectionElements selection;
std::string clipboard;
configElement config;
AiProps AiSettings;
AiUtils AiVars;
SearchElement search;
cursorElement cursor;

int lastModifiedTime = 0;
std::vector<std::string> buffer;
std::vector<std::string> initialFileBuffer;
std::vector<std::string> inlineBuffer;
int inlineBufferPosX = 0;
int inlineBufferPosY = 0;



std::string detectedLang = "";
std::vector<fileElements> fileElementsBuffer;
bool tabOverlayActive = false;
tabOverlayParams tabParams;
bool inplacementHappened = false;


bool unsavedChanges = false;
bool createNewFile = true;
std::string configPath = expandPath("~/.config/idet/config");
int lastEditTime = 0;
std::string filename;
std::vector<cacheAction> cacheActionBuffer; 
int cacheIndex = -1; 
int savedCacheIndex = -1;
std::vector<std::vector<std::string>> inactiveBuffer;
std::vector<std::string> fileList;
int activeBufferIndex = 0;
std::vector<char> openCharList;






void debugWrite(std::ofstream& out, const std::string& msg) {
    if (!out.is_open()) return;

    if (msg.size() <= config.DEBUG_MAX) {
        out << msg;
    } else {
        out << msg.substr(0, config.DEBUG_MAX);
    }
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
        if (tolowerString(std::string(argv[i])) == "--multifile") {
            config.multiFileMode = true;
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
            createNewFile = false;
        }
        else if (arg[0] != '-') {
            
            if (config.multiFileMode) {
                fileList.push_back(arg);
                //std::cerr << "Adding file: " << arg << "\n";
            } else {
                filename = arg;
            }
        }
    }


    if (config.multiFileMode){
        if (fileList.empty()){
            std::cerr << "Multi-file mode enabled but no files provided!\n";
            return 1;
        }
        debugWrite("Files to load: " + strVecToString(fileList));
        filename = fileList[0]; // set first file as main buffer file
        if(config.multiFileMode == true){
            loadInfileElements(fileElementsBuffer, filename);
        }
        
    }
    if (isDirectory(filename)){
        std::cerr << "Provided Filename is a Directory and can not be opened: " + filename + "\n";
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
    debugWrite("Loading File: " + filename);
    if (checkFileExistance(filename)){
        loadFile(filename, buffer, initialFileBuffer, lastModifiedTime);
    }
    else {

        if (createNewFile == true){
            //createNewFileFunc(argv[1]);
            loadFile(filename, buffer , initialFileBuffer, lastModifiedTime);
        }
    }
    
    
    if (config.multiFileMode == true) {
        
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
    // detect lang
    detectLanguage(buffer, detectedLang, filename);


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


    int rowOffset = 0;
    int colOffset = 0;
    int ch;
    

    auto initTime = std::chrono::system_clock::now();
    lastEditTime = std::chrono::system_clock::to_time_t(initTime);
    if(config.multiFileMode){
        SetInfileElements(fileElementsBuffer, activeBufferIndex, lastModifiedTime, unsavedChanges, selection, cursor);
    }
    while (true) {
        inplacementHappened = false;
        int maxVisibleRows = LINES - 2;
        if (cursor.Y - rowOffset >= maxVisibleRows) rowOffset = cursor.Y - maxVisibleRows + 1;
        if (cursor.Y - rowOffset < 0) rowOffset = cursor.Y;
        if (tabOverlayActive && tabParams.exists) {
            tabParams.buffer = buffer;
            tabParams.cursorX = cursor.X;
            tabParams.cursor.Y = cursor.Y;
            debugWrite("Tab overlay parameters updated - cursor: (" + std::to_string(cursor.X) + ", " + std::to_string(cursor.Y) + ")");
        }
        
        draw(cursor, rowOffset,
            filename, config.lineNumberScheme, config.contentScheme,
             unsavedChanges, colOffset,
            AiSettings.inlineSuggestionNPredict, config.multiFileMode, fileList,
            activeBufferIndex, detectedLang, buffer,
            AiVars.showInlineSuggestion, lastModifiedTime,
            tabOverlayActive, tabParams, inlineBuffer,
            inlineBufferPosX, inlineBufferPosY, selection);
        if (search.active) {
            debugWrite("Searching through results...");
            emptySearchOverlay(search.term);
            int ch = waitOnKeyPress();
            debugWrite("Key pressed during search: " + std::to_string(ch));
            if (ch == 10) {
                debugWrite("Enter pressed, moving through results");
                if (!search.results.empty()) {
                    if (search.count >= search.results.size()) {
                        search.count = 0; 
                    }

                    cursor.X = search.results[search.count].x;
                    cursor.Y = search.results[search.count].y;
                    search.count++;
                }
                continue;
            }
            else if (ch == 27) { // ESC
                search.active = false;
                search.results.clear();
                search.count = 0;
                continue;
            }
            else{
                search.active = false;
                search.results.clear();
                search.count = 0;
                continue;
            }
        }
        auto now = std::chrono::system_clock::now();
        std::time_t timeNow = std::chrono::system_clock::to_time_t(now);
        int timeSinceLastEdit = static_cast<int>(timeNow - lastEditTime);
        
        if (timeSinceLastEdit >= AiSettings.AUTO_SUGGESTION_DELAY && !AiVars.autoSuggestionTriggered && 
            !AiVars.showInlineSuggestion && !AiVars.inlineSuggestionExists && AiVars.allowInlineSuggestion) {
            debugWrite("Auto-triggering inline suggestion after " + std::to_string(timeSinceLastEdit) + " seconds");
            getInlineSuggestion(cursor, buffer, AiSettings.maxInlinePromptSize, AiSettings, inlineBuffer, inlineBufferPosX, inlineBufferPosY, AiVars.showInlineSuggestion);
            AiVars.inlineSuggestionExists = true;
            AiVars.autoSuggestionTriggered = true;
            detectLanguage(buffer, detectedLang, filename); 
        }

        ch = getch();
        
        if (ch != ERR) {
            auto now = std::chrono::system_clock::now();
            lastEditTime = std::chrono::system_clock::to_time_t(now);
            AiVars.autoSuggestionTriggered = false;
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
        int oldXPos = cursor.X;
        int oldYPos = cursor.Y;
        

        std::vector<std::string> bufferBeforeAction = buffer;
        
        switch (ch) {
            case 4:{
                if (!buffer.empty() && cursor.Y >= 0 && cursor.Y < static_cast<int>(buffer.size())) {
                    tabParams.buffer = buffer;
                    tabParams.cursor.X = cursor.X;
                    tabParams.cursor.Y = cursor.Y;
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
                if (AiVars.showInlineSuggestion) {
                    AiVars.showInlineSuggestion = false;
                    inlineBuffer.clear();
                    debugWrite("Inline suggestion cancelled with ESC");
                }
                else if (search.active) {
                    search.active = false;
                    debugWrite("Selection cancelled with ESC");
                }
                break;
            case 19: // CTRL+S
                debugWrite("CTRL+S pressed - Saving file");
                saveFile(filename, lastModifiedTime, unsavedChanges, initialFileBuffer, savedCacheIndex, buffer, cacheIndex);
                break;
            case CTRL_KEY('z'):
                undo(cursor, buffer, cacheActionBuffer, cacheIndex, savedCacheIndex, initialFileBuffer, unsavedChanges);
                break;
            case CTRL_KEY('y'):
                redo(cursor, buffer, cacheActionBuffer, cacheIndex, savedCacheIndex, initialFileBuffer, unsavedChanges);
                break;
            case 569:
                debugWrite("CTRL+Tab pressed - Switch to next buffer with active buffer index: " + std::to_string(activeBufferIndex));
                    if (config.multiFileMode && activeBufferIndex < inactiveBuffer.size() - 1) {
                        changeFileElements(fileElementsBuffer,activeBufferIndex,activeBufferIndex + 1,lastModifiedTime,unsavedChanges, cursor ,selection);
                        changeActiveBuffer(inactiveBuffer,buffer, activeBufferIndex, activeBufferIndex + 1);
                        filename = fileList[activeBufferIndex];
                        //activeBufferIndex++;

                        break;
                    }
                    else{
                        debugWrite("No next buffer to switch to");
                        break;
                    }
            case 291:
                debugWrite("CTRL+Tab pressed - Switch to next buffer with active buffer index: " + std::to_string(activeBufferIndex));
                    if (config.multiFileMode && activeBufferIndex < inactiveBuffer.size() - 1) {
                        changeFileElements(fileElementsBuffer,activeBufferIndex,activeBufferIndex + 1,lastModifiedTime,unsavedChanges, cursor, selection);
                        changeActiveBuffer(inactiveBuffer,buffer, activeBufferIndex, activeBufferIndex + 1);
                        filename = fileList[activeBufferIndex];
                        //activeBufferIndex++;

                        break;
                    }
                    else{
                        debugWrite("No next buffer to switch to");
                        break;
                    }
            case 554:
                debugWrite("CTRL+Shift+Tab pressed - Switch to previous buffer with active buffer index: " + std::to_string(activeBufferIndex));
                    if (config.multiFileMode && activeBufferIndex > 0) {
                        changeFileElements(fileElementsBuffer,activeBufferIndex,activeBufferIndex - 1,lastModifiedTime,unsavedChanges, cursor, selection);
                        changeActiveBuffer(inactiveBuffer,buffer, activeBufferIndex,activeBufferIndex - 1);
                        filename = fileList[activeBufferIndex];
                        //activeBufferIndex--;

                        break;
                    }
                    else{
                        debugWrite("No previous buffer to switch to");
                        break;
                    }
            case 290:
                debugWrite("CTRL+Shift+Tab pressed - Switch to previous buffer with active buffer index: " + std::to_string(activeBufferIndex));
                    if (config.multiFileMode && activeBufferIndex > 0) {
                        changeFileElements(fileElementsBuffer,activeBufferIndex,activeBufferIndex - 1,lastModifiedTime,unsavedChanges, cursor , selection);
                        changeActiveBuffer(inactiveBuffer,buffer, activeBufferIndex,activeBufferIndex - 1);
                        filename = fileList[activeBufferIndex];
                        //activeBufferIndex--;

                        break;
                    }
                    else{
                        debugWrite("No previous buffer to switch to");
                        break;
                    }
            case KEY_F(2):
                config.lineNumberScheme = (config.lineNumberScheme == 1) ? 2 : 1;
                config.contentScheme    = (config.contentScheme == 3) ? 4 : 3;
                break;
            case 9:
                // TAB accepts inline suggestion or inserts spaces
                {
                    if (AiVars.inlineSuggestionExists && !inlineBuffer.empty()) {
                        // Accept inline suggestion with Tab
                        debugWrite("Accepting inline suggestion via Tab");
                        for (size_t i = 0; i < inlineBuffer.size(); ++i) {
                            const std::string& line = inlineBuffer[i];
                            if (i == 0) {
                                // First line - insert at current cursor position
                                if (cursor.Y >= buffer.size()) {
                                    buffer.emplace_back("");
                                }
                                std::size_t bytePos = char_to_byte_index(buffer[cursor.Y], cursor.X);
                                if (bytePos > buffer[cursor.Y].size()) {
                                    buffer[cursor.Y].resize(bytePos, ' ');
                                }
                                buffer[cursor.Y].insert(bytePos, line);
                                cursor.X += static_cast<int>(line.size());
                            } else {
                                // Subsequent lines
                                std::size_t bytePos = char_to_byte_index(buffer[cursor.Y], cursor.X);
                                std::string restOfLine = buffer[cursor.Y].substr(bytePos);
                                buffer[cursor.Y] = buffer[cursor.Y].substr(0, bytePos);
                                cursor.Y++;
                                buffer.insert(buffer.begin() + cursor.Y, line + restOfLine);
                                cursor.X = static_cast<int>(line.size());
                            }
                        }
                        unsavedChanges = true;
                        AiVars.showInlineSuggestion = false;
                        AiVars.inlineSuggestionExists = false;
                        inlineBuffer.clear();
                    } else {
                        // No suggestion - insert tab spaces
                        std::size_t bytePos = char_to_byte_index(buffer[cursor.Y], cursor.X);
                        buffer[cursor.Y].insert(bytePos, config.tabSpaces, ' ');
                        cursor.X += config.tabSpaces;
                        unsavedChanges = true;
                    }
                }
                break;
            case 0: {
                if (AiVars.inlineSuggestionExists == false){
                    debugWrite("Tab pressed - Triggering AI Completion");
                    std::vector<std::string> vectorBeforetxt;
                    vectorBeforetxt.reserve(static_cast<size_t>(cursor.Y) + 1);
                    int limitLine = std::max(0, cursor.Y);
                    for (int vecLine = 0; vecLine < limitLine && vecLine < static_cast<int>(buffer.size()); ++vecLine) {
                        vectorBeforetxt.push_back(buffer[vecLine]);
                    }
                    std::string charsBefore;
                    if (cursor.Y >= 0 && cursor.Y < static_cast<int>(buffer.size())) {
                        std::size_t bytePos = char_to_byte_index(buffer[cursor.Y], cursor.X);
                        charsBefore = buffer[cursor.Y].substr(0, bytePos);
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
                            std::size_t bytePos = char_to_byte_index(buffer[cursor.Y], cursor.X);
                            std::string newLine = buffer[cursor.Y].substr(bytePos);
                            buffer[cursor.Y] = buffer[cursor.Y].substr(0, bytePos);
                            buffer.insert(buffer.begin() + cursor.Y + 1, newLine);
                            cursor.Y++;
                            cursor.X = 0;
                            if (cursor.Y >= buffer.size()) {
                                buffer.emplace_back("");
                            }
                            cursor.X = 0;
                            continue;
                        }
                        if (charLlamaOutput >= 32 && charLlamaOutput <= 126) {
                            if (cursor.Y >= buffer.size()) {
                                buffer.emplace_back("");
                            }
                            std::size_t bytePos = char_to_byte_index(buffer[cursor.Y], cursor.X);
                            if (bytePos > buffer[cursor.Y].size()) {
                                buffer[cursor.Y].resize(bytePos, ' ');
                            }
                            buffer[cursor.Y].insert(bytePos, 1, static_cast<char>(charLlamaOutput));
                            cursor.X++;
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
                                if (cursor.Y >= buffer.size()) {
                                    buffer.emplace_back("");
                                }
                                std::size_t bytePos = char_to_byte_index(buffer[cursor.Y], cursor.X);
                                if (bytePos > buffer[cursor.Y].size()) {
                                    buffer[cursor.Y].resize(bytePos, ' ');
                                }
                                buffer[cursor.Y].insert(bytePos, line);
                                cursor.X += static_cast<int>(line.size()); 
                            } else {
                                
                                std::size_t bytePos = char_to_byte_index(buffer[cursor.Y], cursor.X);
                                std::string restOfLine = buffer[cursor.Y].substr(bytePos);
                                buffer[cursor.Y] = buffer[cursor.Y].substr(0, bytePos);
                                cursor.Y++;
                                buffer.insert(buffer.begin() + cursor.Y, line + restOfLine);
                                cursor.X = static_cast<int>(line.size()); 
                            }
                        }
                        unsavedChanges = true;
                    }
                    AiVars.showInlineSuggestion = false;
                    AiVars.inlineSuggestionExists = false;
                    inlineBuffer.clear();
                    break;
                }
            }
            case 570: {
                if (!search.active){
                debugWrite("shift + strg + arrow right");
                search.active = true;
                selection.startX = cursor.X;
                selection.startY = cursor.Y;
                selection.endY = cursor.Y;
                std::string stringAfter = subtractStringLeft(buffer[cursor.Y], cursor.X);
                debugWrite("cursor.Y pos: " + std::to_string(cursor.Y));
                debugWrite("current line content:" + buffer[cursor.Y]);
                std::string onRight = getWordSelectionRight(stringAfter);
                
                debugWrite("OnRight is: " + onRight);
                int moveRight = getUtf8StrLen(onRight);
                cursor.X += moveRight;
                selection.endX = cursor.X;
                break;
                }
                else{
                    debugWrite("shift + strg + arrow right - extending selection");
                    std::string stringAfter = subtractStringLeft(buffer[cursor.Y], cursor.X);
                    debugWrite("cursor.Y pos: " + std::to_string(cursor.Y));
                    debugWrite("current line content:" + buffer[cursor.Y]);
                    std::string onRight = getWordSelectionRight(stringAfter);
                    debugWrite("OnRight is: " + onRight);
                    int moveRight = getUtf8StrLen(onRight);
                    cursor.X += moveRight;
                    selection.endX = cursor.X;
                    debugWrite("selStartX: " + std::to_string(selection.startX) + " selEndX: " + std::to_string(selection.endX));
                    break;
                }
            }
            case 555:{
                if (!search.active){
                debugWrite("shift + strg + arrow left");
                search.active = true;
                selection.startX = cursor.X;
                selection.startY = cursor.Y;
                selection.endY = cursor.Y;
                std::string stringBefore = subtractStringRight(buffer[cursor.Y], cursor.X);
                debugWrite("cursor.Y pos: " + std::to_string(cursor.Y));
                debugWrite("current line content:" + buffer[cursor.Y]);
                std::string onLeft = getWordSelectionLeft(stringBefore);
                debugWrite("OnLeft is: " + onLeft);
                int moveLeft = getUtf8StrLen(onLeft);
                cursor.X -= moveLeft;
                if (cursor.X < 0) cursor.X = 0;
                selection.endX = cursor.X;
                //switchStartEnd(selStartX, selEndX);
                debugWrite("selStartX: " + std::to_string(selection.startX) + " selEndX: " + std::to_string(selection.endX));

                break;
                }
                else{
                    debugWrite("shift + strg + arrow left - extending selection");
                    std::string stringBefore = subtractStringRight(buffer[cursor.Y], cursor.X);
                    debugWrite("cursor.Y pos: " + std::to_string(cursor.Y));
                    debugWrite("current line content:" + buffer[cursor.Y]);
                    std::string onLeft = getWordSelectionLeft(stringBefore);
                    debugWrite("OnLeft is: " + onLeft);
                    int moveLeft = getUtf8StrLen(onLeft);
                    cursor.X -= moveLeft;
                    if (cursor.X < 0) cursor.X = 0;
                    selection.endX = cursor.X;
                    debugWrite("selStartX: " + std::to_string(selection.startX) + " selEndX: " + std::to_string(selection.endX));
                    break;
                }
            }
            case 337:
                // shift + up
                debugWrite(search.active ? "shift + arrow up - extending" : "shift + arrow up");

                if (!search.active) {
                    search.active = true;
                    selection.startX = cursor.X;
                    selection.startY = cursor.Y;
                }

                if (cursor.Y > 0) {
                    cursor.Y--;
                    if (cursor.X > buffer[cursor.Y].size()) {
                        cursor.X = buffer[cursor.Y].size();
                    }
                } else {
                    cursor.X = 0;
                }
                selection.endX = cursor.X;
                selection.endY = cursor.Y;

                break;
            case 336:
                // shift + down
                debugWrite(search.active ? "shift + arrow down - extending" : "shift + arrow down");

                if (!search.active) {
                    search.active = true;
                    selection.startX = cursor.X;
                    selection.startY = cursor.Y;
                }
                if (cursor.Y < buffer.size() - 1) {
                    cursor.Y++;
                    if (cursor.X > buffer[cursor.Y].size()) {
                        cursor.X = buffer[cursor.Y].size();
                    }
                } else {   
                    cursor.X = buffer[cursor.Y].size();
                }
                selection.endX = cursor.X;
                selection.endY = cursor.Y;
                break;
            
            case 540:
                //strg + shift + end
                debugWrite(search.active ? "shift + end - extending" : "shift + end");
                if (!search.active){
                    search.active = true;
                    selection.startX = cursor.X;
                    selection.startY = cursor.Y;
                }
                if (!buffer.empty()) {
                    selection.endX = buffer.back().size();
                    selection.endY = buffer.size() - 1;
                    cursor.X = buffer.back().size();
                    cursor.Y = buffer.size() - 1;
                }

                break;
            case 545:
                //strg + shift + home
                debugWrite(search.active ? "shift + home - extending" : "shift + home");
                if (!search.active){
                    search.active = true;
                    selection.startX = cursor.X;
                    selection.startY = cursor.Y;
                }
                selection.endX = 0;
                selection.endY = 0;
                cursor.X = 0;
                cursor.Y = 0;
                break;
            case 386:
                // shift + end
                debugWrite(search.active ? "shift + end - extending" : "shift + end");
                if (!search.active){
                    search.active = true;
                    selection.startX = cursor.X;
                    selection.startY = cursor.Y;
                }
                selection.endX = buffer[cursor.Y].size();
                selection.endY = cursor.Y;
                cursor.X = buffer[cursor.Y].size();
                break;
            case 391:
                // shift + home
                debugWrite(search.active ? "shift + home - extending" : "shift + home");
                if (!search.active){
                    search.active = true;
                    selection.startX = cursor.X;
                    selection.startY = cursor.Y;
                }
                selection.endX = 0;
                selection.endY = cursor.Y;
                cursor.X = 0;
                break;
            case 402:
                // shift + arrow right
                // adds 1 char to selection and moves cursor
                if (!search.active){
                debugWrite("shift + arrow right");
                search.active = true;
                selection.startX = cursor.X;
                selection.startY = cursor.Y;
                selection.endY = cursor.Y;
                if (cursor.Y < buffer.size()) {
                    std::string stringAfter = subtractStringLeft(buffer[cursor.Y], cursor.X);
                    if (!stringAfter.empty()) {
                        int charLen = getUtf8CharLen(stringAfter, 0);
                        cursor.X += charLen;
                        selection.endX = cursor.X;
                    }
                }
                break;
                }
                else{
                    debugWrite("shift + arrow right - extending selection");
                    if (cursor.Y < buffer.size()) {
                        std::string stringAfter = subtractStringLeft(buffer[cursor.Y], cursor.X);
                        if (!stringAfter.empty()) {
                            int charLen = getUtf8CharLen(stringAfter, 0);
                            cursor.X += charLen;
                            selection.endX = cursor.X;
                            debugWrite("selStartX: " + std::to_string(selection.startX) + " selEndX: " + std::to_string(selection.endX));
                        }
                    }
                    break;
                }
            case 393:
                // shift + arrow left
                // adds 1 char to the left to selection and moves cursor
                if (!search.active){
                debugWrite("shift + arrow left");
                search.active = true;
                selection.startX = cursor.X;
                selection.startY = cursor.Y;
                selection.endY = cursor.Y;
                if (cursor.Y < buffer.size()) {
                    std::string stringBefore = subtractStringRight(buffer[cursor.Y], cursor.X);
                    if (!stringBefore.empty()) {
                        int charLen = getUtf8CharLenReverse(stringBefore);
                        cursor.X -= charLen;
                        if (cursor.X < 0) cursor.X = 0;
                        selection.endX = cursor.X;
                        debugWrite("selStartX: " + std::to_string(selection.startX) + " selEndX: " + std::to_string(selection.endX));
                    }
                }
                break;
                }
                else{
                    debugWrite("shift + arrow left - extending selection");
                    if (cursor.Y < buffer.size()) {
                        std::string stringBefore = subtractStringRight(buffer[cursor.Y], cursor.X);
                        if (!stringBefore.empty()) {
                            int charLen = getUtf8CharLenReverse(stringBefore);
                            cursor.X -= charLen;
                            if (cursor.X < 0) cursor.X = 0;
                            selection.endX = cursor.X;
                            debugWrite("selStartX: " + std::to_string(selection.startX) + " selEndX: " + std::to_string(selection.endX));
                        }
                    }
                    break;
                }

            case KEY_F(1):
                showHelp(version, config.lineNumberScheme);
                break;
            case 544:
                search.active = false;
                cursor.X = 0;
                cursor.Y = 0;
                break;
            case KEY_F(7):

                displayAISettings(cursor, rowOffset, argv[1], config.lineNumberScheme, config.contentScheme, search.active, unsavedChanges, colOffset, AiSettings);
                cursor.X = oldXPos;
                cursor.Y = oldYPos;
                break;
            case 539:
                search.active = false;
                if (!buffer.empty()) {
                    cursor.Y = static_cast<int>(buffer.size() - 1);
                    cursor.X = static_cast<int>(buffer.back().size());
                } else {
                    cursor.Y = 0;
                    cursor.X = 0;
                }
                break;
            case 330:
                if (search.active) {
                    // Delete selected content
                    int startX = selection.startX;
                    int startY = selection.startY;
                    int endX   = selection.endX;
                    int endY   = selection.endY;

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
                    cursor.X = startX;
                    cursor.Y = startY;
                    search.active = false;
                    unsavedChanges = true;
                } else if (cursor.Y >= 0 && cursor.Y < static_cast<int>(buffer.size())) {
                    std::string &line = buffer[cursor.Y];
                    int charCount = getUtf8StrLen(line);
                    if (cursor.X >= 0 && cursor.X < charCount) {
                        // Delete: remove UTF-8 character at cursor
                        std::size_t bytePos = char_to_byte_index(line, cursor.X);
                        int charLen = getUtf8CharLen(line, bytePos);
                        debugWrite("Char len: " + std::to_string(charLen));
                        line.erase(bytePos, charLen > 0 ? charLen : 1);
                    }
                    unsavedChanges = true;
                }
                break;
            case KEY_F(3):
                if (!search.active) {
                    search.active = true;
                    selection.startY = selection.endY = cursor.Y;
                    selection.startX = selection.endX = cursor.X;
                    debugWrite("Selection started at (" +
                            std::to_string(selection.startY) + "," +
                            std::to_string(selection.startX) + ")");
                } else {
                    search.active = false;
                    debugWrite("Selection ended at (" +
                            std::to_string(selection.endY) + "," +
                            std::to_string(selection.endX) + ")");
                }
                break;
            case 18:
                debugWrite("Reloading File");
                erase();
                reloadFile(filename,buffer, initialFileBuffer, lastModifiedTime, cacheActionBuffer, cacheIndex, savedCacheIndex);

                
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
                search.active = true;
                selection.startX = 0;
                selection.startY = 0;
                if (!buffer.empty()) {
                    selection.endX = buffer.back().size();
                    selection.endY = buffer.size() - 1;
                }
                continue;
                break;
            case CTRL_KEY('c'):
                if (search.active) {
                    clipboard.clear();
                    copyClipboard(std::min(selection.startY, selection.endY),
                                std::max(selection.startY, selection.endY), buffer, clipboard, selection);
                    debugWrite("Copied to clipboard: " + clipboard);
                    search.active = false;
                }
                break;
            case CTRL_KEY('v'):
                AiVars.showInlineSuggestion = false;
                AiVars.inlineSuggestionExists = false;
                if (!clipboard.empty()) {
                    pasteClipboard(cursor.Y, cursor.X, buffer, clipboard);
                    debugWrite("Pasted from clipboard at (" +
                            std::to_string(cursor.Y) + "," +
                            std::to_string(cursor.X) + ")");
                }
                break;
            case CTRL_KEY('k'):
                AiVars.showInlineSuggestion = false;
                AiVars.inlineSuggestionExists = false;
                if (cursor.Y < static_cast<int>(buffer.size())) {
                    buffer.erase(buffer.begin() + cursor.Y);
                    if (cursor.Y >= static_cast<int>(buffer.size())) cursor.Y = buffer.size() - 1;
                    if (cursor.Y < 0) {
                        buffer.push_back("");
                        cursor.Y = 0;
                    }
                    cursor.X = 0;
                    unsavedChanges = true;
                }
                break;
            case 272:
                debugWrite(strVecToString(customCommandsBuiltIn));
                break;
            case KEY_UP:
                if (cursor.Y > 0) cursor.Y--;
                cursor.X = std::min(cursor.X, getUtf8StrLen(buffer[cursor.Y]));
                AiVars.showInlineSuggestion = false;
                AiVars.inlineSuggestionExists = false;
                search.active = false;
                break;
            case KEY_DOWN:
                if (cursor.Y < static_cast<int>(buffer.size()) - 1) cursor.Y++;
                AiVars.showInlineSuggestion = false;
                AiVars.inlineSuggestionExists = false;
                search.active = false;
                cursor.X = std::min(cursor.X, getUtf8StrLen(buffer[cursor.Y]));
                break;
            case KEY_LEFT:
                if (cursor.X > 0) cursor.X--;
                AiVars.showInlineSuggestion = false;
                AiVars.inlineSuggestionExists = false;
                search.active = false;
                break;
            case KEY_RIGHT: {
                int charCount = getUtf8StrLen(buffer[cursor.Y]);
                if (cursor.X < charCount) cursor.X++;
                AiVars.showInlineSuggestion = false;
                AiVars.inlineSuggestionExists = false;
                search.active = false;
                break;
            }
            case KEY_HOME:
                cursor.X = 0;
                AiVars.showInlineSuggestion = false;
                AiVars.inlineSuggestionExists = false;
                search.active = false;
                break;
            case KEY_END: {
                search.active = false;
                cursor.X = getUtf8StrLen(buffer[cursor.Y]);
                AiVars.showInlineSuggestion = false;
                AiVars.inlineSuggestionExists = false;
                break;
            }
            case 10: {
                AiVars.showInlineSuggestion = false;
                AiVars.inlineSuggestionExists = false;
                std::size_t bytePos = char_to_byte_index(buffer[cursor.Y], cursor.X);
                std::string newLine = buffer[cursor.Y].substr(bytePos);
                buffer[cursor.Y] = buffer[cursor.Y].substr(0, bytePos);
                buffer.insert(buffer.begin() + cursor.Y + 1, newLine);
                cursor.Y++;
                cursor.X = 0;
                break;
            }
            case KEY_BACKSPACE:
                unsavedChanges = true;
            case 127: {
                AiVars.showInlineSuggestion = false;
                AiVars.inlineSuggestionExists = false;
                unsavedChanges = true;
                if (selection.active) {
                    // Delete selected content
                    int startX = selection.startX;
                    int startY = selection.startY;
                    int endX   = selection.endX;
                    int endY   = selection.endY;

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
                    cursor.X = startX;
                    cursor.Y = startY;
                    selection.active = false;
                } else if (cursor.X > 0) {
                    
                    int spacesBeforeCursor = NdirectspacesBeforeNum(buffer[cursor.Y], cursor.X);
                    int deleteCount = 1; 
                    
                    
                    if (spacesBeforeCursor > 0 && spacesBeforeCursor % config.tabSpaces == 0) {
                        deleteCount = config.tabSpaces;
                    }
                    
                    
                    for (int i = 0; i < deleteCount && cursor.X > 0; i++) {
                        std::size_t bytePos = char_to_byte_index(buffer[cursor.Y], cursor.X);
                        std::size_t prevBytePos = char_to_byte_index(buffer[cursor.Y], cursor.X - 1);
                        buffer[cursor.Y].erase(prevBytePos, bytePos - prevBytePos);
                        cursor.X--;
                    }
                } else if (cursor.Y > 0) {
                    cursor.X = getUtf8StrLen(buffer[cursor.Y - 1]);
                    buffer[cursor.Y - 1] += buffer[cursor.Y];
                    buffer.erase(buffer.begin() + cursor.Y);
                    cursor.Y--;
                }
                break;
            }
            case 6:
                searchOverlay(buffer, cursor.X, cursor.Y, search);
                debugWrite("got results in Vec: " + posCordsVecToString(search.results));
                break;
            default: {
                // Handle selection deletion first if selection is active
                if(search.active){
                    int startX = selection.startX;
                    int startY = selection.startY;
                    int endX   = selection.endX;
                    int endY   = selection.endY;

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
                    cursor.X = startX;
                    cursor.Y = startY;
                    search.active = false;
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
                    if (cursor.X > buffer[cursor.Y].size()) {
                        buffer[cursor.Y].resize(cursor.X, ' ');
                    }
                    std::size_t bytePos = char_to_byte_index(buffer[cursor.Y], cursor.X);
                    buffer[cursor.Y].insert(bytePos, utf8_char);
                    cursor.X += 1;
                    unsavedChanges = true;
                    AiVars.showInlineSuggestion = false;
                    AiVars.inlineSuggestionExists = false;
                    break;
                }
                if (ch >= 32 && ch <= 126) {
                    std::size_t bytePos = char_to_byte_index(buffer[cursor.Y], cursor.X);
                    if (bytePos > buffer[cursor.Y].size()) {
                        buffer[cursor.Y].resize(bytePos, ' ');
                    }
                    char charToInsert = static_cast<char>(ch);
                    buffer[cursor.Y].insert(bytePos, 1, charToInsert);
                    cursor.X += 1;
                    
                    // Auto-close brackets and quotes
                    if (isOpeningChar(charToInsert)) {
                        char closingChar = getClosingChar(charToInsert);
                        if (closingChar != '\0') {
                            std::size_t closeBytePos = char_to_byte_index(buffer[cursor.Y], cursor.X);
                            buffer[cursor.Y].insert(closeBytePos, 1, closingChar);
                            // Cursor stays between opening and closing char
                        }
                    }
                    
                    unsavedChanges = true;
                    AiVars.showInlineSuggestion = false;
                    AiVars.inlineSuggestionExists = false;
                    break;
                }
                break;

            }
        }

        // Store action in cache if buffer changed or position changed
        if (buffer != bufferBeforeAction || cursor.X != oldXPos || cursor.Y != oldYPos || inplacementHappened) {
            
            // Only cache certain actions that change buffer state
            if (ch >= 32 || ch == 10 || ch == KEY_BACKSPACE || ch == 127 || 
                ch == 330 || ch == CTRL_KEY('k') || ch == CTRL_KEY('v') || ch == 9 || (ch < 32 && search.active || inplacementHappened)) {
                
                // For paste operations, track the size of pasted content
                int pasteSize = 0;
                if (ch == CTRL_KEY('v')) {
                    pasteSize = clipboard.size();
                }
                debugWrite("Appending CacheActionBuffer");
                appendCacheActionBuffer(bufferBeforeAction, buffer, ch, cursor.X, cursor.Y, cacheActionBuffer, config.maxCacheNum, cacheIndex, pasteSize);
            }
        }

        // Update selection end
        if (search.active) {
            selection.endY = cursor.Y;
            selection.endX = cursor.X;
        }
    }

    endwin();
    return 0;
}