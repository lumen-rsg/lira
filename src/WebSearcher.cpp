//
// Created by cv2 on 14.12.2025.
//

#include "WebSearcher.h"

#include <iomanip>
#include <ios>
#include <iostream>
#include <regex>
#include <sstream>
#include <curl/curl.h>

#include "Helpers.h"

namespace lira
{
    size_t WebSearcher::WriteCallback(void* contents, const size_t size, const size_t nmemb, std::string* s) {
        const size_t newLength = size * nmemb;
        s->append(static_cast<char*>(contents), newLength);
        return newLength;
    }
    std::string WebSearcher::url_encode(const std::string &value) {
        std::ostringstream escaped;
        escaped.fill('0'); escaped << std::hex;
        for (const char c : value) {
            if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') escaped << c;
            else { escaped << std::uppercase; escaped << '%' << std::setw(2) << static_cast<int>(static_cast<unsigned char>(c)); escaped << std::nouppercase; }
        }
        return escaped.str();
    }

    std::string WebSearcher::perform_search(const std::string& query) {
        std::cout << ANSI_BLUE << "[Searching: " << query << "...]" << ANSI_RESET << std::endl;

        CURL* curl = curl_easy_init();
        std::string buffer;
        if(curl) {
            std::string url = "https://html.duckduckgo.com/html/?q=" + url_encode(query);
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (LiraAgent/1.0)");
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_perform(curl);
            curl_easy_cleanup(curl);
        }
        if(buffer.empty()) return "Error: Network request failed.";

        std::regex snippet_regex(R"(<a[^>]*class="result__snippet"[^>]*>(.*?)</a>)");
        std::string results = "Search Results for: " + query + "\n";
        int count = 0;
        auto begin = std::sregex_iterator(buffer.begin(), buffer.end(), snippet_regex);
        auto end = std::sregex_iterator();

        for (std::sregex_iterator i = begin; i != end; ++i) {
            if(count >= 4) break;
            const std::smatch& match = *i;
            std::string text = match.str(1);
            // Basic cleanup
            text = std::regex_replace(text, std::regex(R"(<[^>]*>)"), "");
            text = std::regex_replace(text, std::regex(R"(&amp;)"), "&");
            text = std::regex_replace(text, std::regex(R"(&quot;)"), "\"");
            text = std::regex_replace(text, std::regex(R"(&lt;)"), "<");
            text = std::regex_replace(text, std::regex(R"(&gt;)"), ">");
            results += std::format("[{}] {}\n", count+1, text);
            count++;
        }

        if (count == 0) {
            std::cout << ANSI_GRAY << "(No results found or parsing failed)" << ANSI_RESET << std::endl;
            return "No text results found.";
        }
        return results;
    }
}
