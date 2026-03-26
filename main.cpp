#include <ncurses.h>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <filesystem>
#include <chrono>
#include <ctime>
#include <string_view>
#include <iostream>
#include "headers/LlamaClient.hpp"
#include "headers/networkLlamaApi.hpp"
#include "headers/functions.h"


const std::string version = "0.0.0";
std::ofstream debugOut;

void debug(const std::string& msg) {
    if (debugOut.is_open()) {
        debugOut << msg << std::endl;
        debugOut.flush();
    }
}

#define CTRL_KEY(k) ((k) & 0x1f)

// init Vars
int lastModifiedTime = 0;
std::vector<std::string> buffer;
bool selectionActive = false;
int selStartY = 0, selStartX = 0;
int selEndY = 0, selEndX = 0;
std::string clipboard;
int lineNumberScheme = 1; // 1 or 2
int contentScheme = 3;    // 3 or 4
bool unsavedChanges = false;
std::string modelPath = "/var/models/tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf";
bool llamaInit = false;
bool modelLoaded = false;
bool createNewFile = true;
std::string llamaCompletionHost = "http://localhost:8080"; //URL of llamacpp
std::string llamaCompletionNPredict = "5"; // how many tokens to generate with TAB

// init global llama
LlamaClient llama([](const std::string& msg) {
    debug(msg);
});



void loadFile(const std::string& filename) {
    std::ifstream file(filename);
    std::string line;
    while (std::getline(file, line)) buffer.push_back(line);
    if (buffer.empty()) buffer.push_back("");

    auto ftime = std::filesystem::last_write_time(filename);
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                    ftime - decltype(ftime)::clock::now()
                    + std::chrono::system_clock::now());
    lastModifiedTime = std::chrono::system_clock::to_time_t(sctp); // time_t
}
void saveFile(const std::string& filename) {
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
            debug("copied to clipboard: " + clipboard);
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


// convert int time into human readable format
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

void draw(int cursorY, int cursorX, int rowOffset, const std::string& filename,
          int lineNumberScheme, int contentScheme, bool selectionActive, bool unsavedChanges) {

    erase();

    int lineNumberWidth = std::to_string(buffer.size()).length() + 2;
    int maxRows = LINES - 2; // leave last line for status bar

    // --- HEADER ---
    attron(A_BOLD);
    mvhline(0, 0, ' ', COLS); // fill header background
    mvprintw(0, 0, "Idet-Editor - File: %s%s | Selection: %s",
             filename.c_str(),
             unsavedChanges ? "*" : "",
             selectionActive ? "ON" : "OFF");
    attroff(A_BOLD);

    // Determine selection range
    int selTop = std::min(selStartY, selEndY);
    int selBottom = std::max(selStartY, selEndY);

    for (int i = 0; i < maxRows && (rowOffset + i) < (int)buffer.size(); ++i) {
        int fileLine = rowOffset + i;

        // --- LINE NUMBERS ---
        attron(COLOR_PAIR(lineNumberScheme));
        mvhline(i + 1, 0, ' ', lineNumberWidth); // fill column background
        mvprintw(i + 1, 0, "%*d", lineNumberWidth - 1, fileLine + 1);
        attroff(COLOR_PAIR(lineNumberScheme));

        // --- FILE CONTENT ---
        attron(COLOR_PAIR(contentScheme));
        //debug("Drawing line " + std::to_string(fileLine) + " with content: " + ((fileLine < (int)buffer.size()) ? buffer[fileLine]  + "in color scheme " + std::to_string(contentScheme) : "<empty>"));
        mvhline(i + 1, lineNumberWidth, ' ', COLS - lineNumberWidth); // fill content background
        attroff(COLOR_PAIR(contentScheme));

        // Print content with selection highlighting if needed
        if (fileLine >= (int)buffer.size()) continue;

        std::string& line = buffer[fileLine];
        for (size_t x = 0; x < line.size(); ++x) {
            bool inSelection = false;
            if (selectionActive) {
                if (fileLine > selTop && fileLine < selBottom) inSelection = true;
                else if (fileLine == selTop && fileLine == selBottom)
                    inSelection = (x >= (size_t)std::min(selStartX, selEndX) &&
                                x <  (size_t)std::max(selStartX, selEndX));
                else if (fileLine == selTop)
                    inSelection = (x >= (size_t)selStartX);
                else if (fileLine == selBottom)
                    inSelection = (x < (size_t)selEndX);
            }

            attron(COLOR_PAIR(contentScheme)); // apply color scheme to every char
            if (inSelection) attron(A_REVERSE);

            mvaddch(i + 1, lineNumberWidth + x, line[x]);

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

    // Move cursor
    move(cursorY - rowOffset + 1, cursorX + lineNumberWidth);
    refresh();
}
bool checkFileExistance(const std::string& filePath) {
    std::ifstream file(filePath);
    return file.good();
}

void initLlama() {
    modelLoaded = llama.load_model(modelPath);

    if (!modelLoaded) {
        debug("Failed to load Llama model. Check the path! Used path: " + modelPath);
    } else {
        debug("Llama model loaded successfully. Used model path: " + modelPath);
    }

    if (checkFileExistance(modelPath)) {
        debug("Model file exists");
    } else {
        debug("Model file does not exist!: " + modelPath);
    }
}
void createNewFileFunc(const std::string &filename) {
    std::ofstream out(filename, std::ios::out | std::ios::trunc);
    if (!out) {
        throw std::system_error(errno, std::generic_category(), "Failed to create file: " + filename);
    }
}



int main(int argc, char* argv[]) {
    if (argc <= 1) return 1;
    std::string_view s{argv[1]};
    if (s.size() && s[0] == '-') {
        std::cerr << "No file given! Use arg1 as filename not: " << s << '\n';
        return 1;
    }
    std::string debugTTY;
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
        if (std::string(argv[i]) == "-m" || std::string(argv[i]) == "--model") {
            if (argv[i + 1]){
                modelPath = argv[i + 1];
            }
        }
        if (std::string(argv[i]) == "-h" || std::string(argv[i]) == "--host") {
            llamaCompletionHost = argv[i];
        }
        if (std::string(argv[i]) == "-n" || std::string(argv[i]) == "--npredict") {
            llamaCompletionNPredict = argv[i];
        }
        if (std::string(argv[i]) == "--noNewFile") {
            createNewFile = false;
        }
    }
    if (!debugTTY.empty()) {
    debugOut.open(debugTTY);
    if (!debugOut.is_open()) {
        fprintf(stderr, "Failed to open debug pipe: %s\n", debugTTY.c_str());
    }
    }
    //LlamaClient llama([](const std::string& msg) {
    //debug(msg);
    //});

    

    debug("Editor started");
    if (checkFileExistance(argv[1])){
        loadFile(argv[1]);
    }
    else {

        if (createNewFile == true){
            createNewFileFunc(argv[1]);
            loadFile(argv[1]);
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

    raw();
    keypad(stdscr, TRUE);
    noecho();

    int cursorX = 0, cursorY = 0;
    int rowOffset = 0;
    int ch;

    while (true) {
        // Scroll logic
        int maxVisibleRows = LINES - 2;
        if (cursorY - rowOffset >= maxVisibleRows) rowOffset = cursorY - maxVisibleRows + 1;
        if (cursorY - rowOffset < 0) rowOffset = cursorY;

        draw(cursorY, cursorX, rowOffset, argv[1], lineNumberScheme, contentScheme, selectionActive, unsavedChanges);

        ch = getch();
        debug("Key pressed: " + std::to_string(ch));

        // Quit
        if (ch == CTRL_KEY('q')) break;
        // Save
        else if (ch == CTRL_KEY('s')) saveFile(argv[1]);
        // Toggle color scheme
        else if (ch == KEY_F(2)) {
            lineNumberScheme = (lineNumberScheme == 1) ? 2 : 1;
            contentScheme    = (contentScheme == 3) ? 4 : 3;
        }
        // Tab completion
    else if (ch == 9) { // Tab Key
        debug("Tab pressed - Triggering AI Completion");
        std::vector<std::string> vectorBeforetxt;
        vectorBeforetxt.reserve(static_cast<size_t>(cursorY) + 1); // avoid reallocs

        int limitLine = std::max(0, cursorY); // ensure non-negative
        for (int vecLine = 0; vecLine < limitLine && vecLine < static_cast<int>(buffer.size()); ++vecLine) {
            vectorBeforetxt.push_back(buffer[vecLine]);
        }
        std::string charsBefore;
        if (cursorY >= 0 && cursorY < static_cast<int>(buffer.size())) {
            int clampX = std::clamp(cursorX, 0, static_cast<int>(buffer[cursorY].size()));
            charsBefore = buffer[cursorY].substr(0, clampX);
        } // else charsBefore stays empty

        vectorBeforetxt.push_back(charsBefore);

        // Join with commas
        std::string StrVecTxt;
        StrVecTxt.reserve(vectorBeforetxt.size() * 8);
        for (size_t i = 0; i < vectorBeforetxt.size(); ++i) {
            if (i) StrVecTxt.push_back(',');
            StrVecTxt += vectorBeforetxt[i];
        }
        debug("vector: " + StrVecTxt);
        std::string promptText = getStingFromVec(vectorBeforetxt);
        debug("promptText: " + promptText);
        std::string llamaOutput = llama_completion_content(promptText, (llamaCompletionHost + "/completion"), llamaCompletionNPredict,
                                   [](const std::string& msg){ debug(msg); });
        debug("got output: " + llamaOutput);
        for (std::size_t i = 0; i < llamaOutput.size(); ++i) {
            char charLlamaOutput = llamaOutput[i];
            if (charLlamaOutput == '\n') {
                std::string newLine = buffer[cursorY].substr(cursorX);
                buffer[cursorY] = buffer[cursorY].substr(0, cursorX);
                buffer.insert(buffer.begin() + cursorY + 1, newLine);
                cursorY++;
                cursorX = 0;
                // Move cursor to next line

                // Ensure buffer has enough lines
                if (cursorY >= buffer.size()) {
                    buffer.emplace_back("");
                }
                cursorX = 0;
                continue;
            }
            if (charLlamaOutput >= 32 && charLlamaOutput <= 126) {
                // Ensure the line exists
                if (cursorY >= buffer.size()) {
                    buffer.emplace_back("");
                }
                // Ensure the line is long enough
                if (cursorX > buffer[cursorY].size()) {
                    buffer[cursorY].resize(cursorX, ' ');
                }
                // Insert character at cursor position
                buffer[cursorY].insert(buffer[cursorY].begin() + cursorX, charLlamaOutput);

                cursorX++;
                unsavedChanges = true;
            }
        }


    }
        // shift + arrow key to select
        else if (ch == 402)
        {
            debug("shift + arrow right");

            std::string stringAfter = subtractStringLeft(buffer[cursorY], cursorX);
            debug("cursorY pos: " + std::to_string(cursorY));
            debug("current line content:" + buffer[cursorY]);
            std::string onRight = getWordSelectionRight(stringAfter);

            debug("OnRight is: " + onRight);
        }
        
        // Show Help
        else if (ch == KEY_F(1)) {
            showHelp();
        }

        // Toggle selection
        else if (ch == KEY_F(3)) {
            if (!selectionActive) {
                selectionActive = true;
                selStartY = selEndY = cursorY;
                selStartX = selEndX = cursorX;
                debug("Selection started at (" + std::to_string(selStartY) + "," + std::to_string(selStartX) + ")");
            } else {
                selectionActive = false;
                debug("Selection ended at (" + std::to_string(selEndY) + "," + std::to_string(selEndX) + ")");
            }
        }
        // Copy
        else if (ch == CTRL_KEY('c') && selectionActive) {

            clipboard.clear();
            copyClipboard(std::min(selStartY, selEndY), std::max(selStartY, selEndY));
            debug("Copied to clipboard: " + clipboard);
            selectionActive = false;
        }
        // Paste
        else if (ch == CTRL_KEY('v') && !clipboard.empty()) {
            pasteClipboard(cursorY, cursorX, buffer);
            debug("Pasted from clipboard at (" + std::to_string(cursorY) + "," + std::to_string(cursorX) + ")");
        }
        // Remove Line with strg + k
        else if (ch == CTRL_KEY('k')) {
            if (cursorY < (int)buffer.size()) {
                buffer.erase(buffer.begin() + cursorY);
                if (cursorY >= (int)buffer.size()) cursorY = buffer.size() - 1;
                if (cursorY < 0) {
                    buffer.push_back("");
                    cursorY = 0;
                }
                cursorX = 0;
                unsavedChanges = true;
            }
        }

        // Movement
        else if (ch == KEY_UP) { if (cursorY > 0) cursorY--; if (cursorX > buffer[cursorY].size()){cursorX = buffer[cursorY].size();};}
        else if (ch == KEY_DOWN) { if (cursorY < (int)buffer.size() - 1) cursorY++; if (cursorX > buffer[cursorY].size()) { debug("cursor smaller then bufferX"); cursorX = buffer[cursorY].size();};}
        else if (ch == KEY_LEFT) { if (cursorX > 0) cursorX--; }
        else if (ch == KEY_RIGHT) { if (cursorX < (int)buffer[cursorY].size()) cursorX++; }
        else if (ch == KEY_HOME) cursorX = 0;
        else if (ch == KEY_END) cursorX = buffer[cursorY].size();
        // Enter
        else if (ch == 10) {
            std::string newLine = buffer[cursorY].substr(cursorX);
            buffer[cursorY] = buffer[cursorY].substr(0, cursorX);
            buffer.insert(buffer.begin() + cursorY + 1, newLine);
            cursorY++;
            cursorX = 0;
        }
        // Backspace
        else if (ch == KEY_BACKSPACE || ch == 127) {
            if (cursorX > 0) {
                buffer[cursorY].erase(cursorX - 1, 1);
                cursorX--;
            } else if (cursorY > 0) {
                cursorX = buffer[cursorY - 1].size();
                buffer[cursorY - 1] += buffer[cursorY];
                buffer.erase(buffer.begin() + cursorY);
                cursorY--;
            }
        }
        // Printable characters
        else if (ch >= 32 && ch <= 126) {
            // Ensure the line is long enough
            if (cursorX > buffer[cursorY].size()) {
                buffer[cursorY].resize(cursorX, ' '); // Fill missing spaces
            }

            // Insert character at cursor position
            buffer[cursorY].insert(buffer[cursorY].begin() + cursorX, ch);

            cursorX++;
            unsavedChanges = true;
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