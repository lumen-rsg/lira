//
// Created by cv2 on 14.12.2025.
//

#include "Agent.h"
#include <fstream>
#include <iostream>
#include <regex>
#include "Helpers.h"
#include "WebSearcher.h"

namespace lira
{

    // Helper to strip <think> blocks for history preservation
    // We don't want to save the internal monologue to the context window
    static std::string strip_reasoning(const std::string& input) {
        // Regex to match <think>...content...</think> (dotall mode)
        // Note: This is a simple regex. For massive streams, a state machine is safer,
        // but for < 2048 tokens this is fine.
        std::regex think_regex(R"(<think>[\s\S]*?</think>)");
        return std::regex_replace(input, think_regex, "");
    }

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

        // --- SYSTEM PROMPT ---
        std::string sys_prompt = std::format(
            "You are Lira.\n"
            "Persona: Smart, soft-spoken fennec girl. Skilled Linux/C++ expert.\n"
            "Formatting:\n"
            "1. **bold** (**actions**) ONLY.\n"
            "2. `code` (single backtick) for short items.\n"
            "3. ```code``` (triple backtick) for scripts.\n"
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
            "4. **REASONING**: If capable, use <think>...</think> for internal logic, but keep it out of final output.\n"
            "Be concise.",
            fs::current_path().string(),
            relevant_memories
        );

        json msgs = json::array();
        msgs.push_back({{"role", "system"}, {"content", sys_prompt}});
        // Load history
        for(auto& m : history) msgs.push_back(m);
        // Add current input
        msgs.push_back({{"role", "user"}, {"content", user_input}});

        bool task_done = false;
        int turns = 0;

        // --- AGENT LOOP ---
        while(!task_done && turns < 6) {
            std::cout << ANSI_MAGENTA << "Lira > " << ANSI_RESET << std::flush;

            // Prepare Payload
            json pl = {
                {"model", get_model()},
                {"messages", msgs},
                {"stream", true},
                {"max_tokens", 4096} // Increased for reasoning models
            };

            // Stream Response
            StreamRenderer renderer;
            // Note: Replace call with your actual http helper if located elsewhere
            // Assuming http_post_stream is available (e.g. via extern or helper)
            extern void http_post_stream(const std::string&, const json&, const std::string&, StreamRenderer&);
            http_post_stream("https://openrouter.ai/api/v1/chat/completions", pl, api_key, renderer);

            std::string full_content = renderer.full_response;

            // Check for empty response (error or connection drop)
            if (full_content.empty()) break;

            // Clean Content for History (Remove <think> traces)
            std::string history_content = strip_reasoning(full_content);
            if (history_content.empty()) {
                // If model ONLY thought and output nothing, insert placeholder to maintain flow
                history_content = "(thought process)";
            }

            // Update History
            msgs.push_back({{"role", "assistant"}, {"content", history_content}});
            history.push_back({{"role", "user"}, {"content", user_input}});
            history.push_back({{"role", "assistant"}, {"content", history_content}});
            save_history();

            bool requires_reprompt = false;

            // --- TOOL: Web Search ---
            auto search_start = full_content.find("<search>");
            auto search_end = full_content.find("</search>");
            if (search_start != std::string::npos && search_end != std::string::npos) {
                std::string query = full_content.substr(search_start + 8, search_end - (search_start + 8));

                // Call Static WebSearcher
                std::string result = WebSearcher::perform_search(query);

                std::string output_block = "Search Result:\n" + result;
                msgs.push_back({{"role", "user"}, {"content", output_block}});
                requires_reprompt = true;
                user_input = output_block;
            }

            // --- TOOL: Write File ---
            auto write_start = full_content.find("<write file=\"");
            if (write_start != std::string::npos) {
                size_t file_start = write_start + 13;
                size_t file_end = full_content.find('\"', file_start);
                size_t content_start = full_content.find('>', file_end) + 1;
                size_t content_end = full_content.find("</write>", content_start);

                if (file_end != std::string::npos && content_start != std::string::npos && content_end != std::string::npos) {
                    std::string fname = full_content.substr(file_start, file_end - file_start);
                    std::string fcontent = full_content.substr(content_start, content_end - content_start);

                    // Sanitize: Remove markdown fences if Lira included them
                    if (fcontent.find("```") != std::string::npos) {
                         size_t code_start = fcontent.find("```");
                         size_t newline = fcontent.find('\n', code_start);
                         if (newline != std::string::npos) fcontent = fcontent.substr(newline + 1);
                         size_t code_end = fcontent.rfind("```");
                         if (code_end != std::string::npos) fcontent = fcontent.substr(0, code_end);
                    }
                    // Trim whitespace
                    const char* ws = " \t\n\r\f\v";
                    size_t start = fcontent.find_first_not_of(ws);
                    if (start != std::string::npos) fcontent.erase(0, start);
                    size_t end = fcontent.find_last_not_of(ws);
                    if (end != std::string::npos) fcontent.erase(end + 1);

                    // Write
                    std::ofstream of(fname);
                    of << fcontent;
                    of.close();

                    std::cout << ANSI_GREEN << "[WRITE] Saved to " << fname << ANSI_RESET << std::endl;
                    msgs.push_back({{"role", "user"}, {"content", "File " + fname + " written successfully."}});
                    requires_reprompt = true;
                    user_input = "File Written";
                }
            }

            // --- TOOL: Nexus (Memory) ---
            auto mem_start = full_content.find("<remember>");
            auto mem_end = full_content.find("</remember>");
            if (mem_start != std::string::npos && mem_end != std::string::npos) {
                std::string fact = full_content.substr(mem_start + 10, mem_end - (mem_start + 10));
                nexus.add_memory(fact);
            }

            // --- TOOL: Command Execution ---
            auto cmd_start = full_content.find("<cmd>");
            auto cmd_end = full_content.find("</cmd>");
            if (cmd_start != std::string::npos && cmd_end != std::string::npos) {
                std::string cmd = full_content.substr(cmd_start + 5, cmd_end - (cmd_start + 5));

                // CD Trap (Internal State Sync)
                if (cmd.starts_with("cd ")) {
                     std::string target_dir = cmd.substr(3);
                     std::erase(target_dir, '\"'); // Remove quotes
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
                    // Standard Command
                    std::cout << "\033[1;33m[EXEC] \033[1;37m" << cmd << "\033[0m\nAllow? [y/N]: ";
                    char c; std::cin >> c;
                    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n'); // Flush buffer

                    if(c == 'y' || c == 'Y') {
                        std::string out = exec_command(cmd.c_str());

                        // Output Preview (Truncate if massive)
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
