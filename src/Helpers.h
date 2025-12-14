//
// Created by cv2 on 14.12.2025.
//
#pragma once
#include <string>
#include <nlohmann/json_fwd.hpp>
#include <filesystem>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <set>

#include "StreamRenderer.h"

namespace lira
{
    // Namespace Aliases
    using json = nlohmann::json;
    namespace fs = std::filesystem;

    // --- Global Constants & Config ---
    // Using inline const to allow definition in header without ODR violations
    inline const std::string BASE_DIR = std::string(getenv("HOME")) + "/.lira";
    inline const std::string SESSIONS_DIR = BASE_DIR + "/sessions";
    inline const std::string NEXUS_FILE = BASE_DIR + "/data/nexus.json";

    // ANSI Colors
    inline const std::string ANSI_RESET   = "\033[0m";
    inline const std::string ANSI_GRAY    = "\033[90m";
    inline const std::string ANSI_CYAN    = "\033[36m";
    const std::string ANSI_MAGENTA = "\033[1;35m";
    const std::string ANSI_BLUE    = "\033[1;34m";
    const std::string ANSI_GREEN   = "\033[0;32m";
    const std::string ANSI_YELLOW  = "\033[1;33m";
    const std::string ANSI_CODE    = "\033[0m";

    // Syntax Highlighting
    const std::string KW_COLOR     = "\033[1;35m";
    const std::string STR_COLOR    = "\033[0;32m";
    const std::string COM_COLOR    = "\033[90m";
    const std::string NUM_COLOR    = "\033[0;33m";

    // Universal Keywords List
    inline const std::set<std::string> KW_SET = {
        "auto", "const", "class", "struct", "int", "void", "string", "bool", "char",
        "if", "else", "for", "while", "do", "return", "switch", "case", "break",
        "namespace", "using", "template", "typename", "public", "private", "protected",
        "import", "from", "def", "lambda", "try", "except", "finally", "with", "as",
        "var", "let", "function", "async", "await", "echo", "sudo", "cd",
        "ls", "grep", "cat", "chmod", "chown", "mkdir", "rm", "mv", "cp", "true", "false",
        "null", "nullptr", "this", "new", "delete", "std", "vector", "map", "set",
        "g++", "gcc", "make", "cmake", "git"
    };

    // --- Helper Functions ---

    inline std::string get_model() {
        const char* env_model = std::getenv("LIRA_MODEL");
        return env_model ? std::string(env_model) : "openai/gpt-4o-mini";
    }


    struct StreamContext {lira::StreamRenderer* renderer{}; std::string buffer; };
    inline size_t StreamCallback(void* ptr, const size_t size, const size_t nmemb, void* userdata) {
        const size_t real_size = size * nmemb;
        auto* ctx = static_cast<StreamContext*>(userdata);
        ctx->buffer.append(static_cast<char*>(ptr), real_size);
        size_t pos;
        while ((pos = ctx->buffer.find('\n')) != std::string::npos) {
            std::string line = ctx->buffer.substr(0, pos);
            ctx->buffer.erase(0, pos + 1);
            if (line.starts_with("data: ")) {
                std::string json_str = line.substr(6);
                if (json_str == "[DONE]") return real_size;
                try {
                    if (json j = json::parse(json_str); j.contains("choices") && !j["choices"].empty()) {
                        if (auto& delta = j["choices"][0]["delta"]; delta.contains("content") && !delta["content"].is_null()) {
                            ctx->renderer->print(delta["content"]);
                        }
                    }
                } catch (...) {}
            }
        }
        return real_size;
    }

    inline void http_post_stream(const std::string& url, const json& payload, const std::string& api_key, lira::StreamRenderer& renderer) {
        CURL* curl = curl_easy_init();
        StreamContext ctx; ctx.renderer = &renderer;
        if(curl) {
            struct curl_slist* headers = nullptr;
            headers = curl_slist_append(headers, ("Authorization: Bearer " + api_key).c_str());
            headers = curl_slist_append(headers, "Content-Type: application/json");
            headers = curl_slist_append(headers, "HTTP-Referer: https://github.com/lira-agent");
            headers = curl_slist_append(headers, "X-Title: Lira Agent");
            const std::string json_str = payload.dump();
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_str.c_str());
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, StreamCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
            curl_easy_perform(curl);
            curl_easy_cleanup(curl);
            curl_slist_free_all(headers);
        }
        renderer.finish();
    }

    inline std::string exec_command(const char* cmd) {
        char buffer[128];
        std::string result;
        const std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
        if (!pipe) return "Failed to run command.";
        while (fgets(buffer, sizeof(buffer), pipe.get()) != nullptr) {
            result += buffer;
        }
        return result;
    }
}