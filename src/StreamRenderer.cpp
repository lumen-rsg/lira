#include "StreamRenderer.h"
#include <iostream>
#include <cctype>
#include <vector>

namespace lira
{
    // --- Constants & Config ---

    // Colors (CLI Only)
    const std::string ANSI_RESET   = "\033[0m";
    const std::string ANSI_GRAY    = "\033[90m";
    const std::string ANSI_CYAN    = "\033[36m";  // Box Border
    const std::string ANSI_MAGENTA = "\033[1;35m";
    const std::string ANSI_BLUE    = "\033[1;34m";
    const std::string ANSI_GREEN   = "\033[0;32m";
    const std::string ANSI_YELLOW  = "\033[1;33m"; // Lang Name
    const std::string ANSI_CODE    = "\033[0m";

    // Syntax Highlighting Colors (CLI Only)
    const std::string KW_COLOR     = "\033[1;35m"; // Magenta
    const std::string STR_COLOR    = "\033[0;32m"; // Green
    const std::string COM_COLOR    = "\033[90m";   // Gray
    const std::string NUM_COLOR    = "\033[0;33m"; // Yellow

    // Universal Keywords List
    const std::set<std::string> KW_SET_IMPL = {
        "auto", "const", "class", "struct", "int", "void", "string", "bool", "char",
        "if", "else", "for", "while", "do", "return", "switch", "case", "break",
        "namespace", "using", "template", "typename", "public", "private", "protected",
        "import", "from", "def", "lambda", "try", "except", "finally", "with", "as",
        "var", "let", "function", "async", "await", "echo", "sudo", "cd",
        "ls", "grep", "cat", "chmod", "chown", "mkdir", "rm", "mv", "cp", "true", "false",
        "null", "nullptr", "this", "new", "delete", "std", "vector", "map", "set",
        "g++", "gcc", "make", "cmake", "git", "npm", "pip", "docker"
    };

    // --- Constructor ---
    StreamRenderer::StreamRenderer(RenderCallback callback)
        : output_callback(std::move(callback)) {}

    // --- Internal Helper: Emit ---
    // Centralizes logic for sending data to CLI (cout) or GUI (callback)
    void StreamRenderer::emit(TokenType type, const std::string& content) {
        if (output_callback) {
            // GUI Mode: Send structured token
            output_callback(type, content);
        } else {
            // CLI Mode: Print with ANSI formatting
            switch (type) {
            case TokenType::Text:
                std::cout << content;
                break;
            case TokenType::Bold:
                // Gray text for actions/bold
                std::cout << ANSI_GRAY << content << ANSI_RESET;
                break;
            case TokenType::InlineCode:
                std::cout << ANSI_CYAN << content << ANSI_RESET;
                break;
            case TokenType::Thinking:
                // Handled separately by render_think_spinner for animation
                // But if we get a static chunk:
                std::cout << ANSI_MAGENTA << content << ANSI_RESET;
                break;
            case TokenType::CodeBlockStart:
                // Top of the box
                std::cout << "\n" << ANSI_CYAN << "╭─[ " << ANSI_YELLOW << content << ANSI_CYAN << " ]" << ANSI_RESET << "\n";
                std::cout << ANSI_CYAN << "│ " << ANSI_RESET;
                break;
            case TokenType::CodeBlockContent:
                // Content is printed raw here; syntax highlighting handles colors before calling this
                // or we assume content already has colors if passed from flush_word (for CLI)
                // However, flush_word calls std::cout directly in CLI mode to handle colors finely.
                // This case is mostly for GUI fallback or uncolored blocks.
                std::cout << content;
                break;
            case TokenType::CodeBlockEnd:
                // Bottom of the box
                std::cout << "\n" << ANSI_CYAN << "╰──────────────────────────────────────────" << ANSI_RESET << "\n";
                break;
            case TokenType::ToolOutput:
                // Usually hidden, but if shown:
                std::cout << ANSI_BLUE << content << ANSI_RESET;
                break;
            }
            std::cout << std::flush;
        }
    }

    // --- Word Buffer Flushing (Syntax Highlighting) ---
    void StreamRenderer::flush_word() {
        if (word_buffer.empty()) return;

        if (output_callback) {
            // GUI Mode: Just send content.
            // Note: Real syntax highlighting in ImGui requires complex logic (e.g. ImGuiColorTextEdit).
            // For now, we send raw text to the code block.
            output_callback(TokenType::CodeBlockContent, word_buffer);
        } else {
            // CLI Mode: Apply ANSI Colors
            if (isdigit(word_buffer[0])) {
                std::cout << NUM_COLOR << word_buffer;
            } else if (KW_SET_IMPL.contains(word_buffer)) {
                std::cout << KW_COLOR << word_buffer;
            } else {
                std::cout << ANSI_CODE << word_buffer;
            }
        }
        word_buffer.clear();
    }

    // --- Thinking Animation ---
    void StreamRenderer::render_think_spinner() {
        if (output_callback) {
            // GUI Mode: Send a status update
            // We use a special token or just update a status line
            output_callback(TokenType::Thinking, "...");
        } else {
            // CLI Mode: Braille Spinner
            const char* spinner[] = { "⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏" };
            int frame = (think_spinner_idx++) % 10;
            // \r to overwrite line, \033[K to clear it
            std::cout << "\r" << ANSI_MAGENTA << spinner[frame] << " (Lira is thinking...)" << ANSI_RESET << std::flush;
        }
    }

    // --- Main Processing Loop ---
    void StreamRenderer::print(const std::string& chunk) {
        full_response += chunk;
        std::string processing = tag_lookahead + chunk;
        tag_lookahead = "";

        // Animate spinner if thinking
        if (in_thinking) {
            render_think_spinner();
        }

        for (size_t i = 0; i < processing.length(); ++i) {
            char c = processing[i];

            // Init cleanup (swallow leading whitespace)
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

            // --- 2. Thinking Logic (<think>) ---
            if (in_thinking) {
                format_buffer += c;
                if (format_buffer.ends_with("</think>")) {
                    in_thinking = false;
                    format_buffer = "";
                    // Clear CLI line
                    if (!output_callback) std::cout << "\r\033[K" << std::flush;
                }
                continue;
            }

            // --- 3. Lookahead ---
            // Prevent splitting tags or markdown tokens across chunks
            if (i >= processing.length() - 2) {
                if (c == '<' || c == '*' || c == '`') {
                    tag_lookahead = processing.substr(i);
                    return;
                }
            }

            // --- 4. Tag Detection ---
            if (c == '<') {
                std::string remainder = processing.substr(i);
                // If we don't have enough chars to decide, lookahead
                if (remainder.length() < 12 && remainder.find('>') == std::string::npos) {
                    tag_lookahead = remainder; return;
                }

                if (remainder.starts_with("<remember>")) { hiding_output = true; target_closer = "</remember>"; format_buffer += c; continue; }
                if (remainder.starts_with("<cmd>")) { hiding_output = true; target_closer = "</cmd>"; format_buffer += c; continue; }
                if (remainder.starts_with("<search>")) { hiding_output = true; target_closer = "</search>"; format_buffer += c; continue; }
                if (remainder.starts_with("<write")) { hiding_output = true; target_closer = "</write>"; format_buffer += c; continue; }

                if (remainder.starts_with("<think>")) {
                    in_thinking = true;
                    format_buffer += c;
                    render_think_spinner();
                    continue;
                }
            }

            // Capture visible content
            visible_response += c;

            // --- 5. Code Block Boundaries (```) ---
            if (processing.substr(i).starts_with("```")) {
                if (!in_code_block) {
                    // STARTING BLOCK
                    in_code_block = true;
                    awaiting_lang_name = true;
                    lang_buffer = "";
                    // If CLI, ensure separation
                    if (!output_callback) std::cout << "\n";
                } else {
                    // ENDING BLOCK
                    if (awaiting_lang_name) {
                        // Edge case: One-line block ` ```cmd content... `
                        emit(TokenType::CodeBlockStart, "Command");
                        emit(TokenType::CodeBlockContent, lang_buffer);
                        awaiting_lang_name = false;
                    } else {
                        flush_word(); // Flush remaining code buffer
                    }

                    in_code_block = false;
                    in_string = false;
                    in_comment = false;

                    emit(TokenType::CodeBlockEnd, "");
                }
                i += 2; continue;
            }

            // --- 6. Code Block Content ---
            if (in_code_block) {
                // Phase A: Language Name (e.g. "cpp")
                if (awaiting_lang_name) {
                    if (c == '\n') {
                        awaiting_lang_name = false;
                        std::string label = lang_buffer.empty() ? "Code" : lang_buffer;
                        emit(TokenType::CodeBlockStart, label);
                    } else {
                        lang_buffer += c;
                    }
                    continue;
                }

                // Phase B: Left Border & Newlines
                if (c == '\n') {
                    flush_word();
                    in_string = false;
                    in_comment = false;

                    if (output_callback) {
                        output_callback(TokenType::CodeBlockContent, "\n");
                    } else {
                        std::cout << ANSI_RESET << "\n" << ANSI_CYAN << "│ " << ANSI_RESET;
                    }
                    continue;
                }

                // Phase C: Syntax Highlighting
                if (in_string) {
                    if (output_callback) output_callback(TokenType::CodeBlockContent, std::string(1, c));
                    else { std::cout << STR_COLOR << c; }

                    if (c == string_char) in_string = false;
                    continue;
                }
                if (in_comment) {
                    if (output_callback) output_callback(TokenType::CodeBlockContent, std::string(1, c));
                    else { std::cout << COM_COLOR << c; }
                    continue;
                }

                // Start String
                if (c == '"' || c == '\'') {
                    flush_word();
                    in_string = true;
                    string_char = c;

                    if (output_callback) output_callback(TokenType::CodeBlockContent, std::string(1, c));
                    else { std::cout << STR_COLOR << c; }
                    continue;
                }

                // Start Comment
                if (c == '#' || (c == '/' && i+1 < processing.length() && processing[i+1] == '/')) {
                    flush_word();
                    in_comment = true;

                    if (output_callback) output_callback(TokenType::CodeBlockContent, std::string(1, c));
                    else { std::cout << COM_COLOR << c; }
                    continue;
                }

                // Token Buffer
                if (isalnum(c) || c == '_') {
                    word_buffer += c;
                } else {
                    flush_word();
                    if (output_callback) output_callback(TokenType::CodeBlockContent, std::string(1, c));
                    else { std::cout << ANSI_CODE << c; }
                }
                continue;
            }

            // --- 7. Normal Text Formatting ---
            if (!in_code_block) {
                // **bold**
                if (processing.substr(i).starts_with("**")) {
                    in_gray_block = !in_gray_block;
                    i += 1; continue;
                }
                // *action* (single star, unless list item)
                if (processing.substr(i).starts_with("*") && !in_gray_block) {
                    bool is_list = (i + 1 < processing.length() && processing[i+1] == ' ');
                    if (!is_list) {
                        in_gray_block = !in_gray_block;
                        continue;
                    }
                }
                // `inline code`
                if (c == '`') {
                    in_inline_code = !in_inline_code;
                    continue;
                }

                // Emit based on state
                if (in_gray_block) {
                    emit(TokenType::Bold, std::string(1, c));
                } else if (in_inline_code) {
                    emit(TokenType::InlineCode, std::string(1, c));
                } else {
                    emit(TokenType::Text, std::string(1, c));
                }
            }
        }

        if (!output_callback) std::cout << std::flush;
    }

    void StreamRenderer::finish() const {
        if(!tag_lookahead.empty()) {
            if (output_callback) output_callback(TokenType::Text, tag_lookahead);
            else std::cout << tag_lookahead;
        }

        // Ensure "thinking" line is cleared if stream ended abruptly
        if (in_thinking && !output_callback) {
            std::cout << "\r\033[K";
        }

        if (!output_callback) std::cout << ANSI_RESET << std::endl;
    }
}