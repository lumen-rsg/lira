//
// Created by cv2 on 14.12.2025.
//

#include "Agent.h"

#include <fstream>
#include <iostream>

#include "Helpers.h"
#include "WebSearcher.h"

namespace lira
{
    Agent::Agent(const std::string& session_name) {
        const char* env_p = std::getenv("OPENROUTER_API_KEY");
        if(!env_p) { std::cerr << "Need OPENROUTER_API_KEY env var."; exit(1); }
        api_key = env_p;
        api_key.erase(std::ranges::remove_if(api_key, [](unsigned char x){return std::isspace(x);}).begin(), api_key.end());

        std::filesystem::create_directories(SESSIONS_DIR);
        history_path = SESSIONS_DIR + "/" + session_name + ".json";
        load_history();
        std::cout << "\033[1;30m[Session: " << session_name << " loaded]\033[0m" << std::endl;
    }

    void Agent::load_history() {
        if (fs::exists(history_path)) {
            std::ifstream f(history_path);
            try { history = json::parse(f); } catch(...) { history = json::array(); }
        } else { history = json::array(); }
    }

    void Agent::save_history() {
        if (history.size() > 20) {
            json new_h = json::array();
            for(size_t i=history.size()-20; i<history.size(); ++i) new_h.push_back(history[i]);
            history = new_h;
        }
        std::ofstream o(history_path);
        o << history.dump(4);
    }

    void Agent::process(std::string user_input) {
        std::string relevant_memories = nexus.retrieve_relevant(user_input);

        // --- UPDATED PROMPT ---
        std::string sys_prompt = std::format(
            "You are Lira.\n"
            "Persona: Smart, soft-spoken fennec girl. Skilled Linux/C++ expert.\n"
            "Formatting Rules:\n"
            "1. **bold** (**actions**) ONLY.\n"
            "2. `code` (single backtick) for short items/flags/filenames.\n"
            "3. ```code``` (triple backtick) for multi-line scripts ONLY.\n"
            "4. NO emojis.\n"
            "System: Linux/Bash. CWD: {}\n"
            "Nexus:\n{}\n"
            "Tools (Hidden):\n"
            "- <cmd>command</cmd> : Execute shell.\n"
            "- <write file=\"path\">content</write> : Write file (Auto-sanitized).\n"
            "- <search>query</search> : DuckDuckGo Search.\n"
            "- <remember>fact</remember> : Save info.\n"
            "RULES:\n"
            "1. **SILENT EXECUTION**: Output ONLY the tag to use a tool.\n"
            "2. **NO HALLUCINATION**: Search before guessing specs.\n"
            "3. **PLATONIC ONLY**: If sexual topics arise, **(ears droop)** and refuse.\n"
            "Be concise.",
            fs::current_path().string(),
            relevant_memories
        );

        json msgs = json::array();
        msgs.push_back({{"role", "system"}, {"content", sys_prompt}});
        for(auto& m : history) msgs.push_back(m);
        msgs.push_back({{"role", "user"}, {"content", user_input}});

        bool task_done = false;
        int turns = 0;

        while(!task_done && turns < 6) {
            std::cout << ANSI_MAGENTA << "Lira > " << ANSI_RESET << std::flush;

            json pl = {
                {"model", get_model()},
                {"messages", msgs},
                {"stream", true},
                {"max_tokens", 2048}
            };

            StreamRenderer renderer;
            http_post_stream("https://openrouter.ai/api/v1/chat/completions", pl, api_key, renderer);

            std::string content = renderer.full_response;
            if (content.empty()) break;

            msgs.push_back({{"role", "assistant"}, {"content", content}});
            history.push_back({{"role", "user"}, {"content", user_input}});
            history.push_back({{"role", "assistant"}, {"content", content}});
            save_history();

            bool requires_reprompt = false;

            // 1. Web Search
            auto search_start = content.find("<search>");
            auto search_end = content.find("</search>");
            if (search_start != std::string::npos && search_end != std::string::npos) {
                std::string query = content.substr(search_start + 8, search_end - (search_start + 8));
                std::string result = WebSearcher::perform_search(query);
                std::string output_block = "Search Result:\n" + result;
                msgs.push_back({{"role", "user"}, {"content", output_block}});
                requires_reprompt = true;
                user_input = output_block;
            }

            // 2. Write File (Sanitized)
            auto write_start = content.find("<write file=\"");
            if (write_start != std::string::npos) {
                size_t file_start = write_start + 13;
                size_t file_end = content.find('\"', file_start);
                size_t content_start = content.find('>', file_end) + 1;
                size_t content_end = content.find("</write>", content_start);

                if (file_end != std::string::npos && content_start != std::string::npos && content_end != std::string::npos) {
                    std::string fname = content.substr(file_start, file_end - file_start);
                    std::string fcontent = content.substr(content_start, content_end - content_start);

                    // --- SANITIZATION START ---
                    // Strip markdown code fences if Lira put them inside the tag
                    if (fcontent.find("```") != std::string::npos) {
                         // Find first occurrence
                         size_t code_start = fcontent.find("```");
                         // Find newline after first ``` (e.g. ```cpp\n)
                         size_t newline = fcontent.find('\n', code_start);
                         if (newline != std::string::npos) {
                             fcontent = fcontent.substr(newline + 1);
                         }
                         // Find last occurrence
                         size_t code_end = fcontent.rfind("```");
                         if (code_end != std::string::npos) {
                             fcontent = fcontent.substr(0, code_end);
                         }
                    }
                    // Trim leading/trailing whitespace
                    const char* ws = " \t\n\r\f\v";
                    size_t start = fcontent.find_first_not_of(ws);
                    if (start != std::string::npos) fcontent.erase(0, start);
                    size_t end = fcontent.find_last_not_of(ws);
                    if (end != std::string::npos) fcontent.erase(end + 1);
                    // --- SANITIZATION END ---

                    std::ofstream of(fname);
                    of << fcontent;
                    of.close();

                    std::cout << ANSI_GREEN << "[WRITE] Saved to " << fname << ANSI_RESET << std::endl;
                    msgs.push_back({{"role", "user"}, {"content", "File " + fname + " written successfully."}});
                    requires_reprompt = true;
                    user_input = "File Written";
                }
            }

            // 3. Nexus
            auto mem_start = content.find("<remember>");
            auto mem_end = content.find("</remember>");
            if (mem_start != std::string::npos && mem_end != std::string::npos) {
                std::string fact = content.substr(mem_start + 10, mem_end - (mem_start + 10));
                nexus.add_memory(fact);
            }

            // 4. Command
            auto cmd_start = content.find("<cmd>");
            auto cmd_end = content.find("</cmd>");
            if (cmd_start != std::string::npos && cmd_end != std::string::npos) {
                std::string cmd = content.substr(cmd_start + 5, cmd_end - (cmd_start + 5));
                if (cmd.starts_with("cd ")) {
                     std::string target_dir = cmd.substr(3);
                     std::erase(target_dir, '\"');
                     try {
                         fs::current_path(target_dir);
                         std::cout << ANSI_BLUE << "[CWD] Changed to " << fs::current_path().string() << ANSI_RESET << std::endl;
                         msgs.push_back({{"role", "user"}, {"content", "Directory changed to " + fs::current_path().string()}});
                         requires_reprompt = true;
                         user_input = "Directory Changed";
                     } catch(const fs::filesystem_error& e) {
                         msgs.push_back({{"role", "user"}, {"content", "Failed to change directory: " + std::string(e.what())}});
                         requires_reprompt = true;
                         user_input = "CD Failed";
                     }
                } else {
                    std::cout << "\033[1;33m[EXEC] \033[1;37m" << cmd << "\033[0m\nAllow? [y/N]: ";
                    char c; std::cin >> c;
                    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                    if(c == 'y' || c == 'Y') {
                        std::string out = exec_command(cmd.c_str());
                        std::string out_prev = out.length() > 500 ? out.substr(0,500) + "\n...(truncated)" : out;
                        std::cout << "\033[0;32m" << out_prev << "\033[0m\n";
                        std::string output_block = "Output:\n" + out;
                        msgs.push_back({{"role", "user"}, {"content", output_block}});
                        requires_reprompt = true;
                        user_input = output_block;
                    } else {
                        msgs.push_back({{"role", "user"}, {"content", "User denied."}});
                        requires_reprompt = true;
                        user_input = "(User Denied)";
                    }
                }
            }
            if(!requires_reprompt) task_done = true;
            else turns++;
        }
    }
}
