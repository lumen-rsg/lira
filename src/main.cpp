#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <cstdlib>
#include <sstream>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <unistd.h>
#include "Agent.h"
#include "Helpers.h"

int main(int argc, char* argv[]) {
    std::string session = "main";
    std::string one_shot_input;

    // Parse Flags
    for (int i = 1; i < argc; ++i) {
        if (std::string arg = argv[i]; arg == "-s" || arg == "--session") { if (i + 1 < argc) session = argv[++i]; }
        else one_shot_input += arg + " ";
    }

    // Handle Pipe
    if (!isatty(STDIN_FILENO)) {
        std::string line;
        while (std::getline(std::cin, line)) one_shot_input += line + "\n";
    }

    lira::Agent agent(session);

    // One-Shot Mode
    if (!one_shot_input.empty()) {
        agent.process(one_shot_input);
        return 0;
    }

    // Interactive Mode
    std::cout << lira::ANSI_CYAN << "Lira Interactive Mode " << lira::ANSI_GRAY << "(type 'exit' to quit)" << lira::ANSI_RESET << std::endl;
    std::string line;
    while(true) {
        std::cout << lira::ANSI_MAGENTA << "You > " << lira::ANSI_RESET;
        if (!std::getline(std::cin, line)) break;
        if (line == "exit" || line == "quit") break;
        if (line.empty()) continue;
        agent.process(line);
    }
    return 0;
}

