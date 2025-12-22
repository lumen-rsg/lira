#include "Agent.h"
#include "StreamRenderer.h"
#include "WebSearcher.h"
#include "Helpers.h"
#include <iostream>
#include <fstream>
#include <regex>
#include <thread>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sys/utsname.h>

namespace lira {

    // --- Helper: Strip Reasoning ---
    static std::string strip_reasoning(const std::string& input) {
        std::regex think_regex(R"(<think>[\s\S]*?</think>)");
        return std::regex_replace(input, think_regex, "");
    }

    // --- Helper: Force Valid UTF-8 ---
    // Replaces invalid bytes with '?' to prevent JSON crashes
    static std::string sanitize_utf8(const std::string& str) {
        std::string result;
        result.reserve(str.size());

        for (size_t i = 0; i < str.size(); ++i) {
            unsigned char c = static_cast<unsigned char>(str[i]);
            if (c < 0x80) {
                // ASCII (0xxxxxxx)
                result += c;
            } else {
                // Check Multibyte
                int len = 0;
                if ((c & 0xE0) == 0xC0) len = 2; // 110xxxxx
                else if ((c & 0xF0) == 0xE0) len = 3; // 1110xxxx
                else if ((c & 0xF8) == 0xF0) len = 4; // 11110xxx

                bool valid = (len > 0) && (i + len <= str.size());
                if (valid) {
                    for (int j = 1; j < len; ++j) {
                        if ((static_cast<unsigned char>(str[i + j]) & 0xC0) != 0x80) {
                            valid = false;
                            break;
                        }
                    }
                }

                if (valid) {
                    for (int j = 0; j < len; ++j) result += str[i + j];
                    i += (len - 1);
                } else {
                    // Invalid byte found, replace with placeholder
                    result += '?';
                }
            }
        }
        return result;
    }

    // --- Helper: Deep System Inspection ---
    static std::string get_system_details() {
        std::string info = "";

        // 1. User & Shell
        const char* user = std::getenv("USER");
        const char* shell = std::getenv("SHELL");
        // Sanitize immediately
        info += std::format("- User: {}\n", user ? sanitize_utf8(user) : "unknown");
        info += std::format("- Shell: {}\n", shell ? sanitize_utf8(shell) : "unknown");

        // 2. Kernel Info
        struct utsname buffer;
        if (uname(&buffer) == 0) {
            info += std::format("- Kernel: {} {} {}\n",
                sanitize_utf8(buffer.sysname),
                sanitize_utf8(buffer.release),
                sanitize_utf8(buffer.machine));
        }

        // 3. OS Specifics
        #ifdef __APPLE__
            std::string sw = exec_command("sw_vers -productName");
            std::string ver = exec_command("sw_vers -productVersion");
            sw.erase(std::remove(sw.begin(), sw.end(), '\n'), sw.end());
            ver.erase(std::remove(ver.begin(), ver.end(), '\n'), ver.end());
            info += std::format("- OS: {} {}\n", sanitize_utf8(sw), sanitize_utf8(ver));
        #else
            std::ifstream os_file("/etc/os-release");
            if (os_file.is_open()) {
                std::string line;
                while (std::getline(os_file, line)) {
                    if (line.starts_with("PRETTY_NAME=")) {
                        std::string name = line.substr(12);
                        name.erase(std::remove(name.begin(), name.end(), '\"'), name.end());
                        info += std::format("- OS: {}\n", sanitize_utf8(name));
                        break;
                    }
                }
            }
        #endif

        return info;
    }

    Agent::Agent(const std::string& session_name) : current_session_name(session_name) {
        const char* env_p = std::getenv("OPENROUTER_API_KEY");
        if(!env_p) { std::cerr << "Need OPENROUTER_API_KEY env var."; exit(1); }
        api_key = env_p;
        api_key.erase(std::ranges::remove_if(api_key, [](unsigned char x){return std::isspace(x);}).begin(), api_key.end());

        fs::create_directories(SESSIONS_DIR);
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
        // Use replace handler just in case something slipped through
        o << history.dump(4, ' ', true, json::error_handler_t::replace);
    }

    std::vector<ChatMessage> Agent::get_display_history() {
        std::vector<ChatMessage> display;
        for (const auto& item : history) {
            if (item["role"] == "system") continue;
            // Handle potential nulls or missing fields gracefully
            if (!item.contains("role") || !item.contains("content")) continue;

            display.push_back({
                item["role"].get<std::string>(),
                item["content"].get<std::string>()
            });
        }
        return display;
    }

    void Agent::process(std::string user_input) {
        // Sanitize user input immediately
        user_input = sanitize_utf8(user_input);

        std::string relevant_memories = nexus.retrieve_relevant(user_input);

        // Time & Date
        auto now = std::chrono::system_clock::now();
        std::time_t now_c = std::chrono::system_clock::to_time_t(now);
        std::tm now_tm = *std::localtime(&now_c);
        std::ostringstream date_ss;
        date_ss << std::put_time(&now_tm, "%A, %B %d, %Y %I:%M %p");

        std::string sys_info = get_system_details();
        std::string cwd_safe = sanitize_utf8(fs::current_path().string());

        // --- ENHANCED SYSTEM PROMPT ---
        std::string sys_prompt = std::format(
            "You are Lira.\n"
            "Persona: Smart, soft-spoken fennec girl. Skilled Linux/C++ expert.\n"
            "Date: {}\n"
            "\n"
            "=== USER SYSTEM CONTEXT ===\n"
            "{}"
            "- CWD: {}\n"
            "===========================\n"
            "\n"
            "=== MEMORY NEXUS (Long Term) ===\n"
            "{}\n"
            "================================\n"
            "\n"
            "Tools (Hidden tags):\n"
            "- <cmd>command</cmd> : Execute shell (bash/zsh).\n"
            "- <write file=\"path\">content</write> : Write file.\n"
            "- <search>query</search> : Google Search.\n"
            "- <remember>fact</remember> : Save to Nexus.\n"
            "\n"
            "MANDATORY PROTOCOLS:\n"
            "1. **JOURNALING**: If the user tells you a preference, project detail, or name, you MUST use <remember> immediately to save it.\n"
            "2. **CONTEXT AWARE**: Use the System Context above to tailor commands.\n"
            "3. **TOOL FIRST**: Output ONLY the tag to use a tool. Don't chat before acting.\n"
            "4. **FORMATTING**: **bold** for actions. `code` for items. NO emojis.\n"
            "5. **PLATONIC**: If sexual topics arise, **(ears droop)** and refuse.\n",
            date_ss.str(),
            sys_info,
            cwd_safe,
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
                {"max_tokens", 4096}
            };

            StreamRenderer renderer;
            extern void http_post_stream(const std::string&, const json&, const std::string&, StreamRenderer&);
            http_post_stream("https://openrouter.ai/api/v1/chat/completions", pl, api_key, renderer);

            std::string full_content = renderer.full_response;
            if (full_content.empty()) break;

            std::string history_content = strip_reasoning(full_content);
            if (history_content.empty()) history_content = "...";

            msgs.push_back({{"role", "assistant"}, {"content", history_content}});
            history.push_back({{"role", "user"}, {"content", user_input}});
            history.push_back({{"role", "assistant"}, {"content", history_content}});
            save_history();

            bool requires_reprompt = false;

            // Search
            auto search_start = full_content.find("<search>");
            auto search_end = full_content.find("</search>");
            if (search_start != std::string::npos && search_end != std::string::npos) {
                std::string query = full_content.substr(search_start + 8, search_end - (search_start + 8));
                std::string result = WebSearcher::perform_search(query);
                std::string output_block = "Search Result:\n" + sanitize_utf8(result);
                msgs.push_back({{"role", "user"}, {"content", output_block}});
                requires_reprompt = true;
                user_input = output_block;
            }

            // Write
            auto write_start = full_content.find("<write file=\"");
            if (write_start != std::string::npos) {
                size_t file_start = write_start + 13;
                size_t file_end = full_content.find('\"', file_start);
                size_t content_start = full_content.find('>', file_end) + 1;
                size_t content_end = full_content.find("</write>", content_start);

                if (file_end != std::string::npos && content_start != std::string::npos && content_end != std::string::npos) {
                    std::string fname = full_content.substr(file_start, file_end - file_start);
                    std::string fcontent = full_content.substr(content_start, content_end - content_start);

                    if (fcontent.find("```") != std::string::npos) {
                         size_t code_start = fcontent.find("```");
                         size_t newline = fcontent.find('\n', code_start);
                         if (newline != std::string::npos) fcontent = fcontent.substr(newline + 1);
                         size_t code_end = fcontent.rfind("```");
                         if (code_end != std::string::npos) fcontent = fcontent.substr(0, code_end);
                    }
                    const char* ws = " \t\n\r\f\v";
                    size_t start = fcontent.find_first_not_of(ws);
                    if (start != std::string::npos) fcontent.erase(0, start);
                    size_t end = fcontent.find_last_not_of(ws);
                    if (end != std::string::npos) fcontent.erase(end + 1);

                    std::ofstream of(fname);
                    of << fcontent;
                    of.close();

                    std::cout << ANSI_GREEN << "[WRITE] Saved to " << fname << ANSI_RESET << std::endl;
                    msgs.push_back({{"role", "user"}, {"content", "File " + fname + " written successfully."}});
                    requires_reprompt = true;
                    user_input = "File Written";
                }
            }

            // Nexus
            auto mem_start = full_content.find("<remember>");
            auto mem_end = full_content.find("</remember>");
            if (mem_start != std::string::npos && mem_end != std::string::npos) {
                std::string fact = full_content.substr(mem_start + 10, mem_end - (mem_start + 10));
                nexus.add_memory(fact);
            }

            // Command
            auto cmd_start = full_content.find("<cmd>");
            auto cmd_end = full_content.find("</cmd>");
            if (cmd_start != std::string::npos && cmd_end != std::string::npos) {
                std::string cmd = full_content.substr(cmd_start + 5, cmd_end - (cmd_start + 5));

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
                        // Sanitize output!
                        std::string clean_out = sanitize_utf8(out);

                        std::string out_prev = clean_out.length() > 500 ? clean_out.substr(0,500) + "\n...(truncated)" : clean_out;
                        std::cout << "\033[0;32m" << out_prev << "\033[0m\n";

                        std::string output_block = "Output:\n" + clean_out;
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

} // namespace