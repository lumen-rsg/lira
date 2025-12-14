//
// Created by cv2 on 14.12.2025.
//

#pragma once
#include <string>

namespace lira
{
    class StreamRenderer {
        bool is_first_char = true;

        // Formatting States
        bool in_gray_block = false;
        bool in_inline_code = false;

        // Code Block States
        bool in_code_block = false;
        bool awaiting_lang_name = false;
        std::string lang_buffer;

        // Syntax Highlighting States
        bool in_string = false;
        char string_char = 0;
        bool in_comment = false;
        std::string word_buffer;

        // Tool States
        bool hiding_output = false;
        std::string target_closer;
        std::string tag_lookahead;
        std::string format_buffer;

        void flush_word();

    public:
        std::string full_response;

        void print(const std::string& chunk);
        void finish() const;
    };
}