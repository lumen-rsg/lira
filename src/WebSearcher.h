//
// Created by cv2 on 14.12.2025.
//

#pragma once
#include <string>

namespace lira
{
    class WebSearcher {
        static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* s);
        static std::string url_encode(const std::string &value);
    public:
        static std::string perform_search(const std::string& query);
    };
}
