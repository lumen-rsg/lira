#pragma once
#include <iostream>
#include <string>
#include <set>
#include <functional> // Added

namespace lira
{
    // Define a callback for outputting parsed text segments
    // Type: Enum (Text, Code, Info, Error), Content, Color info
    enum class TokenType {
        Text,
        Bold,
        InlineCode,
        CodeBlockStart,
        CodeBlockContent,
        CodeBlockEnd,
        Thinking,
        ToolOutput
    };

    using RenderCallback = std::function<void(TokenType, const std::string&)>;

    class StreamRenderer {
        // ... (Keep existing bool states) ...
        bool is_first_char = true;
        bool in_gray_block = false;
        bool in_inline_code = false;
        bool in_code_block = false;
        bool awaiting_lang_name = false;
        std::string lang_buffer;
        bool in_string = false;
        char string_char = 0;
        bool in_comment = false;
        std::string word_buffer;
        bool hiding_output = false;
        std::string target_closer;
        std::string tag_lookahead;
        std::string format_buffer;
        bool in_thinking = false;
        int think_spinner_idx = 0;

        RenderCallback output_callback; // The hook

        void flush_word();
        void render_think_spinner();

    public:
        std::string full_response;
        std::string visible_response;

        // Constructor accepts a callback.
        // If nullptr, defaults to std::cout (CLI mode)
        explicit StreamRenderer(RenderCallback callback = nullptr);
        void emit(TokenType type, const std::string& content);

        void print(const std::string& chunk);
        void finish() const;
    };
}