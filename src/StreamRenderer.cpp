#include "StreamRenderer.h"
#include <iostream>
#include <cctype>
#include "Helpers.h"

namespace lira
{
    void StreamRenderer::flush_word() {
        if (word_buffer.empty()) return;
        if (isdigit(word_buffer[0])) std::cout << NUM_COLOR << word_buffer;
        else if (KW_SET.contains(word_buffer)) std::cout << KW_COLOR << word_buffer;
        else std::cout << ANSI_CODE << word_buffer;
        word_buffer.clear();
    }

    void StreamRenderer::render_think_spinner() {
        // Braille pattern spinner
        const char* spinner[] = { "⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏" };
        int frame = (think_spinner_idx++) % 10;
        // \r returns to start of line, \033[K clears the line
        std::cout << "\r" << ANSI_MAGENTA << spinner[frame] << " (Lira is thinking...)" << ANSI_RESET << std::flush;
    }

    void StreamRenderer::print(const std::string& chunk) {
        full_response += chunk;
        std::string processing = tag_lookahead + chunk;
        tag_lookahead = "";

        // If we are deep in thought, update spinner per chunk to animate it
        if (in_thinking) {
            render_think_spinner();
        }

        for (size_t i = 0; i < processing.length(); ++i) {
            char c = processing[i];

            if (is_first_char) {
                if (c == '\t' || c == ' ' || c == '\n' || c == '\r') continue;
                is_first_char = false;
            }

            // --- 1. Tool Hiding Logic ---
            if (hiding_output) {
                format_buffer += c;
                if (format_buffer.ends_with(target_closer)) {
                    hiding_output = false;
                    format_buffer = "";
                    target_closer = "";
                }
                continue;
            }

            // --- 2. Thinking/Reasoning Logic ---
            if (in_thinking) {
                format_buffer += c;
                if (format_buffer.ends_with("</think>")) {
                    in_thinking = false;
                    format_buffer = "";
                    // Clear the "Lira is thinking..." line completely
                    std::cout << "\r\033[K" << std::flush;
                }
                continue;
            }

            // --- 3. Lookahead ---
            if (i >= processing.length() - 2) {
                if (c == '<' || c == '*' || c == '`') {
                    tag_lookahead = processing.substr(i);
                    return;
                }
            }

            // --- 4. Tag Detection ---
            if (c == '<') {
                std::string remainder = processing.substr(i);
                if (remainder.length() < 12 && remainder.find('>') == std::string::npos) {
                    tag_lookahead = remainder; return;
                }
                if (remainder.starts_with("<remember>")) { hiding_output = true; target_closer = "</remember>"; format_buffer += c; continue; }
                if (remainder.starts_with("<cmd>")) { hiding_output = true; target_closer = "</cmd>"; format_buffer += c; continue; }
                if (remainder.starts_with("<search>")) { hiding_output = true; target_closer = "</search>"; format_buffer += c; continue; }
                if (remainder.starts_with("<write")) { hiding_output = true; target_closer = "</write>"; format_buffer += c; continue; }

                // Reasoning Tag
                if (remainder.starts_with("<think>")) {
                    in_thinking = true;
                    format_buffer += c;
                    render_think_spinner(); // Initial draw
                    continue;
                }
            }

            // Only append to visible response if not hidden/thinking
            visible_response += c;

            // --- 5. Code Block Boundaries ---
            if (processing.substr(i).starts_with("```")) {
                if (!in_code_block) {
                    in_code_block = true;
                    awaiting_lang_name = true;
                    lang_buffer = "";
                    std::cout << "\n";
                } else {
                    if (awaiting_lang_name) {
                        std::cout << ANSI_CYAN << "╭─[ " << ANSI_YELLOW << "Command" << ANSI_CYAN << " ]" << ANSI_RESET << "\n";
                        std::cout << ANSI_CYAN << "│ " << ANSI_RESET << lang_buffer;
                        awaiting_lang_name = false;
                    } else {
                        flush_word();
                    }
                    in_code_block = false;
                    in_string = false;
                    in_comment = false;
                    std::cout << ANSI_RESET << "\n" << ANSI_CYAN << "╰──────────────────────────────────────────" << ANSI_RESET << "\n";
                }
                i += 2; continue;
            }

            // --- 6. Code Block Content ---
            if (in_code_block) {
                if (awaiting_lang_name) {
                    if (c == '\n') {
                        awaiting_lang_name = false;
                        std::string label = lang_buffer.empty() ? "Code" : lang_buffer;
                        std::cout << ANSI_CYAN << "╭─[ " << ANSI_YELLOW << label << ANSI_CYAN << " ]" << ANSI_RESET << "\n";
                        std::cout << ANSI_CYAN << "│ " << ANSI_RESET;
                    } else {
                        lang_buffer += c;
                    }
                    continue;
                }
                if (c == '\n') {
                    flush_word(); in_string = false; in_comment = false;
                    std::cout << ANSI_RESET << "\n" << ANSI_CYAN << "│ " << ANSI_RESET;
                    continue;
                }
                if (in_string) { std::cout << STR_COLOR << c; if (c == string_char) in_string = false; continue; }
                if (in_comment) { std::cout << COM_COLOR << c; continue; }
                if (c == '"' || c == '\'') { flush_word(); in_string = true; string_char = c; std::cout << STR_COLOR << c; continue; }
                if (c == '#' || (c == '/' && i+1 < processing.length() && processing[i+1] == '/')) { flush_word(); in_comment = true; std::cout << COM_COLOR << c; continue; }
                if (isalnum(c) || c == '_') { word_buffer += c; }
                else { flush_word(); std::cout << ANSI_CODE << c; }
                continue;
            }

            // --- 7. Normal Text Formatting ---
            if (!in_code_block) {
                // **bold**
                if (processing.substr(i).starts_with("**")) {
                    in_gray_block = !in_gray_block;
                    std::cout << (in_gray_block ? ANSI_GRAY : ANSI_RESET);
                    i += 1; continue;
                }
                // `code`
                if (c == '`') {
                    in_inline_code = !in_inline_code;
                    std::cout << (in_inline_code ? ANSI_CYAN : ANSI_RESET);
                    continue;
                }
            }
            std::cout << c;
        }
        std::cout << std::flush;
    }

    void StreamRenderer::finish() const {
        if(!tag_lookahead.empty()) std::cout << tag_lookahead;
        // Ensure line is cleared if we got stuck in thinking
        if (in_thinking) std::cout << "\r\033[K";
        std::cout << ANSI_RESET << std::endl;
    }
}