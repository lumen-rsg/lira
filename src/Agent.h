//
// Created by cv2 on 14.12.2025.
//

#pragma once
#include <string>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include "Nexus.h"

namespace lira
{

    struct ChatMessage {
        std::string role;
        std::string content;
    };

    class Agent {
        std::string api_key;
        // json history; // Changed to public access via getter or friend, see below.
        // Actually, let's keep it private but provide a converter.
        std::string history_path;
        Nexus nexus;

        void load_history();
        void save_history();

    public:
        nlohmann::json history; // Made public for direct GUI access (simplifies binding)
        std::string current_session_name;

        explicit Agent(const std::string& session_name);
        void process(std::string user_input);

        // Helper to get clean vector for GUI
        std::vector<ChatMessage> get_display_history();
    };
}
