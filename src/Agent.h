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
    class Agent {
        std::string api_key;
        nlohmann::json history;
        std::string history_path;
        Nexus nexus;

        void load_history();
        void save_history();

    public:
        explicit Agent(const std::string& session_name);
        void process(std::string user_input);
    };
}
