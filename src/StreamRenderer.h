#pragma once
#include <iostream>
#include <string>
#include <set>

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

        // Reasoning State
        bool in_thinking = false;
        int think_spinner_idx = 0;

        void flush_word();
        void render_think_spinner();

    public:
        std::string full_response; // Raw response (includes tags)
        std::string visible_response; // Cleaned response (no thoughts/tags)

        void print(const std::string& chunk);
        void finish() const;
    };
}