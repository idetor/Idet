#pragma once
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
#include <thread>
// provides help for bash syntax highlighting and autocompletion

struct itemAffiliation {
    bool builtInCommands;
    bool builtInKeyword;
    bool inScriptDefinition;
    bool isCommand;
    bool other;
};

struct SaveAffiliation {
    int StartPosX;
    int StartPosY;
    int EndPosX;
    int EndPosY;
    itemAffiliation affiliation;
};

std::vector<SaveAffiliation> syntaxHighlightingAffiliation;

void getAllCommands(std::vector<std::string>& commands) {
    std::array<char, 256> buffer;

    FILE* pipe = popen("bash -c 'compgen -c'", "r");
    if (!pipe) return;

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        std::string line(buffer.data());

        if (!line.empty() && line.back() == '\n')
            line.pop_back();

        commands.push_back(line);
    }

    pclose(pipe);
}

std::vector<std::string> builtInCommands = {
  "alias",
  "bg",
  "bind",
  "break",
  "builtin",
  "cd",
  "command",
  "compgen",
  "complete",
  "compopt",
  "continue",
  "declare",
  "dirs",
  "disown",
  "echo",
  "enable",
  "eval",
  "exec",
  "exit",
  "export",
  "false",
  "fc",
  "fg",
  "getopts",
  "hash",
  "help",
  "history",
  "jobs",
  "kill",
  "let",
  "local",
  "logout",
  "popd",
  "printf",
  "pushd",
  "pwd",
  "read",
  "readonly",
  "return",
  "set",
  "shift",
  "shopt",
  "source",
  "suspend",
  "test",
  "times",
  "trap",
  "true",
  "type",
  "typeset",
  "ulimit",
  "umask",
  "unalias",
  "unset",
  "wait",
  "case",
  "coproc",
  "function",
  "for",
  "if",
  "select",
  "time",
  "until",
  "while",
  "{",
  "[["
};
std::vector<std::string> builtInKeyword = {
  "do",
  "done",
  "elif",
  "else",
  "esac",
  "fi",
  "in",
  "then",
  "}",
  "]]",
  "!",
  "case",
  "coproc",
  "function",
  "for",
  "if",
  "select",
  "time",
  "until",
  "while",
  "{",
  "[[",
  "]]"
};
std::vector<std::string> bashOperators = {
    "|", "||", "&", "&&", ";", ";;", "(", ")", "{", "}", "[", "]", "<", ">", "<<", ">>"
};
std::vector<std::string> customCommandsBuiltIn;
std::vector<std::string> inScriptDefinitions;

// Track comment positions per line: {lineNum, startPos}
std::vector<std::pair<int, int>> commentPositions;

bool isOperator(const std::string& word) {
    return std::find(bashOperators.begin(), bashOperators.end(), word) != bashOperators.end();
}

bool isCustomCommand(const std::string& word) {
    return std::find(customCommandsBuiltIn.begin(), customCommandsBuiltIn.end(), word) != customCommandsBuiltIn.end();
}

itemAffiliation getAffiliation(std::string word){
    itemAffiliation backAffiliation;
    backAffiliation.builtInCommands = false;
    backAffiliation.builtInKeyword = false;
    backAffiliation.inScriptDefinition = false;
    backAffiliation.isCommand = false;
    backAffiliation.other = false;

    // Remove trailing pipes, redirects, and semicolons for checking
    std::string cleanWord = word;
    while (!cleanWord.empty() && (cleanWord.back() == '|' || cleanWord.back() == '>' || 
           cleanWord.back() == '<' || cleanWord.back() == ';' || cleanWord.back() == '&')) {
        cleanWord.pop_back();
    }

    if (std::find(builtInCommands.begin(), builtInCommands.end(), cleanWord) != builtInCommands.end()){
        backAffiliation.builtInCommands = true;
    }
    else if (isCustomCommand(cleanWord)){
        backAffiliation.builtInCommands = true; // treat custom commands as built-in for highlighting purposes
    }
    else{
        backAffiliation.builtInCommands = false;
    }
    if (std::find(builtInKeyword.begin(), builtInKeyword.end(), cleanWord) != builtInKeyword.end()){
        backAffiliation.builtInKeyword = true;
    } else{
        backAffiliation.builtInKeyword = false;
    }
    backAffiliation.other = !(backAffiliation.builtInCommands || backAffiliation.builtInKeyword);
    return backAffiliation;
}

void appendInScriptDefinition(std::string line, int lineNum){
    // appends a functionname if its valid/in the line
    size_t pos = line.find("() {");
    if (pos != std::string::npos){
        std::string funcName = line.substr(0, pos);
        // remove leading spaces
        funcName.erase(funcName.begin(), std::find_if(funcName.begin(), funcName.end(), [](unsigned char ch) {
            return !std::isspace(ch);
        }));
        inScriptDefinitions.push_back(funcName);
    }
}

void detectInLineAffiliation(std::string line, int lineNum){
    // Find comment position in this line
    size_t commentStart = line.find('#');
    
    // detects if there are any built in commands or keywords in the line and saves their affiliation and position for syntax highlighting
    std::vector<std::string> words;
    size_t pos = 0;
    
    // Split line into words and operators
    while (pos < line.size()) {
        // Stop processing if we hit a comment
        if (commentStart != std::string::npos && pos >= commentStart) {
            break;
        }
        
        // Skip spaces
        while (pos < line.size() && line[pos] == ' ') {
            pos++;
        }
        
        if (pos >= line.size()) break;
        
        // Check for multi-character operators first
        std::string twoChar = line.substr(pos, 2);
        std::string oneChar = line.substr(pos, 1);
        
        size_t start = pos;
        size_t end = pos + 1;
        
        // Check for two-character operators
        if ((twoChar == "||" || twoChar == "&&" || twoChar == "<<" || twoChar == ">>") && isOperator(twoChar)) {
            end = pos + 2;
            SaveAffiliation saveAff;
            saveAff.StartPosX = static_cast<int>(pos);
            saveAff.StartPosY = lineNum;
            saveAff.EndPosX = static_cast<int>(pos + 2);
            saveAff.EndPosY = lineNum;
            saveAff.affiliation.builtInCommands = false;
            saveAff.affiliation.builtInKeyword = false;
            saveAff.affiliation.isCommand = true; // Mark as operator
            syntaxHighlightingAffiliation.push_back(saveAff);
            pos += 2;
            continue;
        }
        // Check for single-character operators
        else if (isOperator(oneChar)) {
            SaveAffiliation saveAff;
            saveAff.StartPosX = static_cast<int>(pos);
            saveAff.StartPosY = lineNum;
            saveAff.EndPosX = static_cast<int>(pos + 1);
            saveAff.EndPosY = lineNum;
            saveAff.affiliation.builtInCommands = false;
            saveAff.affiliation.builtInKeyword = false;
            saveAff.affiliation.isCommand = true; // Mark as operator
            syntaxHighlightingAffiliation.push_back(saveAff);
            pos++;
            continue;
        }
        
        // Get start of word
        start = pos;
        
        // Find end of word (next space or operator or comment)
        while (pos < line.size() && line[pos] != ' ' && !isOperator(line.substr(pos, 1)) && 
               (commentStart == std::string::npos || pos < commentStart)) {
            pos++;
        }
        
        // Extract word
        std::string word = line.substr(start, pos - start);
        if (!word.empty()) {
            itemAffiliation affiliation = getAffiliation(word);
            if (affiliation.builtInCommands || affiliation.builtInKeyword) {
                SaveAffiliation saveAff;
                saveAff.StartPosX = static_cast<int>(start);
                saveAff.StartPosY = lineNum;
                saveAff.EndPosX = static_cast<int>(start + word.size());
                saveAff.EndPosY = lineNum;
                saveAff.affiliation = affiliation;
                syntaxHighlightingAffiliation.push_back(saveAff);
            }
        }
    }
    
    // Store comment position if found
    if (commentStart != std::string::npos) {
        commentPositions.push_back({lineNum, static_cast<int>(commentStart)});
    }
}
bool isCommand(int posX, int posY){
    for (const auto& aff : syntaxHighlightingAffiliation){
        if (aff.affiliation.builtInCommands && aff.StartPosY == posY && posX >= aff.StartPosX && posX < aff.EndPosX){
            return true;
        }
    }
    return false;
}
bool isKeyword(int posX, int posY){
    for (const auto& aff : syntaxHighlightingAffiliation){
        if (aff.affiliation.builtInKeyword && aff.StartPosY == posY && posX >= aff.StartPosX && posX < aff.EndPosX){
            return true;
        }    }
    return false;
}
bool isInScriptDefinition(int posX, int posY){
    for (const auto& aff : syntaxHighlightingAffiliation){
        if (aff.affiliation.inScriptDefinition && aff.StartPosY == posY && posX >= aff.StartPosX && posX < aff.EndPosX){
            return true;
        }    }
    return false;
}

bool isOperatorAt(int posX, int posY){
    for (const auto& aff : syntaxHighlightingAffiliation){
        if (aff.affiliation.isCommand && !aff.affiliation.builtInCommands && !aff.affiliation.builtInKeyword 
            && aff.StartPosY == posY && posX >= aff.StartPosX && posX < aff.EndPosX){
            return true;
        }
    }
    return false;
}

bool isInComment(int posX, int posY){
    for (const auto& comment : commentPositions){
        if (comment.first == posY && posX >= comment.second){
            return true;
        }
    }
    return false;
}