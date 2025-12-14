//
// Created by cv2 on 14.12.2025.
//

#include "Nexus.h"

#include <fstream>
#include <sstream>

#include "Helpers.h"

namespace lira
{
    Nexus::Nexus() {
        if (std::filesystem::exists(NEXUS_FILE)) {
            std::ifstream f(NEXUS_FILE);
            try { memories = json::parse(f); } catch(...) { memories = json::array(); }
        } else { memories = json::array(); }
    }
    void Nexus::save() const
    {
        fs::create_directories(fs::path(NEXUS_FILE).parent_path());
        std::ofstream o(NEXUS_FILE);
        o << memories.dump(4);
    }
    void Nexus::add_memory(const std::string& content) {
        for (const auto& m : memories) if (m.get<std::string>() == content) return;
        memories.push_back(content);
        save();
    }
    std::string Nexus::retrieve_relevant(const std::string& query) {
        if (memories.empty()) return "No memories yet.";
        std::set<std::string> query_tokens;
        std::stringstream ss(query);
        std::string word;
        while (ss >> word) {
            std::erase_if(word, ::ispunct);
            if(word.length() > 3) query_tokens.insert(word);
        }
        std::vector<std::pair<int, std::string>> ranked;
        for (const auto& item : memories) {
            auto mem_str = item.get<std::string>();
            int score = 0;
            for (const auto& token : query_tokens) if (mem_str.find(token) != std::string::npos) score++;
            if (score > 0) ranked.emplace_back(score, mem_str);
        }
        std::sort(ranked.rbegin(), ranked.rend());
        std::string context;
        int count = 0;
        for (const auto& val : ranked | std::views::values) {
            if (count++ >= 3) break;
            context += "- " + val + "\n";
        }
        return context.empty() ? "No relevant memories found." : context;
    }
}
