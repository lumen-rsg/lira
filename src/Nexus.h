//
// Created by cv2 on 14.12.2025.
//

#pragma once
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

namespace lira
{
    class Nexus {
        nlohmann::json memories;
    public:
        Nexus();
        void save() const;
        void add_memory(const std::string& content);
        std::string retrieve_relevant(const std::string& query);
    };
}
