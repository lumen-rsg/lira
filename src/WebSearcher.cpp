#include "WebSearcher.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <regex>
#include <curl/curl.h>
#include <vector>

namespace lira {

    size_t WebSearcher::WriteCallback(void* contents, size_t size, size_t nmemb, std::string* s) {
        size_t newLength = size * nmemb;
        try { s->append(static_cast<char*>(contents), newLength); } catch(...) { return 0; }
        return newLength;
    }

    std::string WebSearcher::url_encode(const std::string &value) {
        std::ostringstream escaped;
        escaped.fill('0'); escaped << std::hex;
        for (char c : value) {
            if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') escaped << c;
            else { escaped << std::uppercase; escaped << '%' << std::setw(2) << static_cast<int>(static_cast<unsigned char>(c)); escaped << std::nouppercase; }
        }
        return escaped.str();
    }

    std::string WebSearcher::perform_search(const std::string& query) {
        std::cout << "\033[1;34m[Searching Google: " << query << "...]\033[0m" << std::endl;

        CURL* curl = curl_easy_init();
        std::string buffer;

        if(curl) {
            // Google Basic Version (gbv=1) - Legacy HTML, no JS, very stable structure
            // hl=en forces English results
            // num=5 limits results to 5
            std::string url = "https://www.google.com/search?q=" + url_encode(query) + "&gbv=1&hl=en&num=5";

            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);

            // Generic Chrome User Agent to avoid immediate 403
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");

            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

            // Important: Google checks referrers sometimes
            curl_easy_setopt(curl, CURLOPT_REFERER, "https://www.google.com/");

            CURLcode res = curl_easy_perform(curl);
            curl_easy_cleanup(curl);

            if(res != CURLE_OK) return "Error: Network request failed.";
        }

        if(buffer.empty()) return "Error: Empty response.";

        // --- Parsing Google Basic HTML ---
        // In gbv=1, main results text is often inside <div class="BNeawe ...">
        // We capture the text content of these divs.

        std::string results_text = "Search Results for: " + query + "\n\n";
        int count = 0;

        // Regex to find the specific Google Basic class "BNeawe"
        // This class wraps Titles (s3v9rd) and Snippets.
        std::regex snippet_regex(R"(<div class="BNeawe[^>]*>(.*?)</div>)");

        auto begin = std::sregex_iterator(buffer.begin(), buffer.end(), snippet_regex);
        auto end = std::sregex_iterator();

        std::vector<std::string> clean_hits;

        for (std::sregex_iterator i = begin; i != end; ++i) {
            std::smatch match = *i;
            std::string text = match.str(1);

            // Cleanup HTML tags
            text = std::regex_replace(text, std::regex(R"(<[^>]*>)"), "");

            // Cleanup Entities
            text = std::regex_replace(text, std::regex(R"(&nbsp;)"), " ");
            text = std::regex_replace(text, std::regex(R"(&amp;)"), "&");
            text = std::regex_replace(text, std::regex(R"(&quot;)"), "\"");
            text = std::regex_replace(text, std::regex(R"(&#39;)"), "'");
            text = std::regex_replace(text, std::regex(R"(&lt;)"), "<");
            text = std::regex_replace(text, std::regex(R"(&gt;)"), ">");

            // Filter out navigation noise (Google puts "Web Images..." in these divs too)
            if (text.length() < 15) continue;
            if (text.find("Google Home") != std::string::npos) continue;
            if (text.find("Settings") != std::string::npos) continue;

            // Deduplicate (Title and Snippet often repeat parts)
            bool duplicate = false;
            for(const auto& h : clean_hits) {
                if (h == text || text.find(h) != std::string::npos) { duplicate = true; break; }
            }
            if(duplicate) continue;

            clean_hits.push_back(text);
            results_text += std::format("[{}] {}\n", count + 1, text);
            count++;

            if(count >= 5) break;
        }

        if (count == 0) return "No readable results. Google might be blocking automated requests.";

        return results_text;
    }

} // namespace lira