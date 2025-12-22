#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <algorithm>
#include "Agent.h" // Includes ChatMessage struct
#include "Helpers.h"

// --- Styling Constants ---
ImVec4 col_bg;
ImVec4 col_sidebar;
ImVec4 col_accent;
ImVec4 col_input_bg;
ImVec4 col_text;

ImU32 col_lira_bg;
ImU32 col_lira_text;
ImU32 col_user_bg;
ImU32 col_user_text;
ImU32 col_shadow = IM_COL32(0, 0, 0, 80);

// --- Global State ---
struct AppState {
    // Settings
    bool settings_open = false;
    int current_theme = 1;
    char model[128] = "openai/gpt-4o-mini";
    char apiKey[512] = "";

    // Session Management
    bool new_session_open = false;
    char new_session_buf[64] = "";
    std::vector<std::string> session_list;
    std::unique_ptr<lira::Agent> active_agent;

    AppState() {
        const char* env_key = std::getenv("OPENROUTER_API_KEY");
        if (env_key) strncpy(apiKey, env_key, sizeof(apiKey) - 1);

        // Load initial session
        refresh_sessions();
        std::string start_session = "main";
        if (!session_list.empty()) start_session = session_list[0];
        active_agent = std::make_unique<lira::Agent>(start_session);
    }

    void refresh_sessions() {
        session_list.clear();
        if (std::filesystem::exists(lira::SESSIONS_DIR)) {
            for (const auto& entry : std::filesystem::directory_iterator(lira::SESSIONS_DIR)) {
                if (entry.path().extension() == ".json") {
                    session_list.push_back(entry.path().stem().string());
                }
            }
        }
        // Sort specifically to keep order consistent
        std::sort(session_list.begin(), session_list.end());
    }

    void switch_session(const std::string& name) {
        active_agent = std::make_unique<lira::Agent>(name);
    }
};

AppState app;
ImFont* font_regular = nullptr;
ImFont* font_input   = nullptr;

// --- Theme System ---
void SetTheme(int index) {
    app.current_theme = index;
    switch (index) {
        case 0: // Kawaii
            col_bg = ImVec4(0.98f, 0.94f, 0.96f, 1.00f); col_sidebar = ImVec4(0.95f, 0.90f, 0.94f, 1.00f); col_accent = ImVec4(1.00f, 0.60f, 0.75f, 1.00f); col_input_bg = ImVec4(1.00f, 1.00f, 1.00f, 1.00f); col_text = ImVec4(0.30f, 0.20f, 0.25f, 1.00f);
            col_lira_bg = IM_COL32(255, 255, 255, 255); col_lira_text = IM_COL32(80, 50, 70, 255); col_user_bg = IM_COL32(255, 153, 190, 255); col_user_text = IM_COL32(255, 255, 255, 255);
            break;
        case 1: // Lira (Dark)
            col_bg = ImVec4(0.12f, 0.12f, 0.14f, 1.00f); col_sidebar = ImVec4(0.08f, 0.08f, 0.09f, 1.00f); col_accent = ImVec4(0.86f, 0.58f, 0.94f, 1.00f); col_input_bg = ImVec4(0.18f, 0.18f, 0.20f, 1.00f); col_text = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
            col_lira_bg = IM_COL32(245, 245, 245, 255); col_lira_text = IM_COL32(20, 20, 20, 255); col_user_bg = IM_COL32(45, 45, 48, 255); col_user_text = IM_COL32(240, 240, 240, 255);
            break;
        case 2: // Aria
            col_bg = ImVec4(0.05f, 0.08f, 0.12f, 1.00f); col_sidebar = ImVec4(0.03f, 0.05f, 0.08f, 1.00f); col_accent = ImVec4(0.00f, 0.80f, 0.90f, 1.00f); col_input_bg = ImVec4(0.08f, 0.12f, 0.18f, 1.00f); col_text = ImVec4(0.90f, 0.95f, 1.00f, 1.00f);
            col_lira_bg = IM_COL32(20, 30, 45, 255); col_lira_text = IM_COL32(220, 240, 255, 255); col_user_bg = IM_COL32(0, 150, 180, 255); col_user_text = IM_COL32(255, 255, 255, 255);
            break;
        case 3: // Lumina
            col_bg = ImVec4(0.94f, 0.94f, 0.92f, 1.00f); col_sidebar = ImVec4(0.90f, 0.90f, 0.88f, 1.00f); col_accent = ImVec4(0.95f, 0.60f, 0.00f, 1.00f); col_input_bg = ImVec4(1.00f, 1.00f, 1.00f, 1.00f); col_text = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
            col_lira_bg = IM_COL32(255, 255, 255, 255); col_lira_text = IM_COL32(40, 40, 40, 255); col_user_bg = IM_COL32(230, 230, 230, 255); col_user_text = IM_COL32(20, 20, 20, 255);
            break;
    }
    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_WindowBg] = col_bg; style.Colors[ImGuiCol_ChildBg] = col_bg; style.Colors[ImGuiCol_Text] = col_text;
    style.Colors[ImGuiCol_Button] = ImVec4(col_accent.x, col_accent.y, col_accent.z, 0.6f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(col_accent.x, col_accent.y, col_accent.z, 0.8f);
    style.Colors[ImGuiCol_ButtonActive] = col_accent;
    style.Colors[ImGuiCol_FrameBg] = col_input_bg; style.Colors[ImGuiCol_Header] = col_accent;
}

// --- Renderers ---

void RenderMessageBubble(const lira::ChatMessage& msg, float width_avail) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    bool is_lira = (msg.role == "assistant"); // Lira is 'assistant' in JSON
    float pad_x = 20.0f, pad_y = 15.0f;
    float max_w = (width_avail * 0.75f) - (pad_x * 2);

    ImVec2 txt_sz = ImGui::CalcTextSize(msg.content.c_str(), nullptr, false, max_w);
    ImVec2 bubble_sz(txt_sz.x + (pad_x * 2), txt_sz.y + (pad_y * 2) + 20.0f);

    float shift_x = is_lira ? 0.0f : width_avail - bubble_sz.x;
    ImVec2 start = ImGui::GetCursorScreenPos();
    ImGui::Dummy(bubble_sz);

    ImVec2 box_min(start.x + shift_x, start.y);
    ImVec2 box_max(box_min.x + bubble_sz.x, box_min.y + bubble_sz.y);

    dl->AddRectFilled(ImVec2(box_min.x+5, box_min.y+5), ImVec2(box_max.x+5, box_max.y+5), col_shadow, 6.0f);
    dl->AddRectFilled(box_min, box_max, is_lira ? col_lira_bg : col_user_bg, 6.0f);

    ImGui::SetCursorScreenPos(ImVec2(box_min.x + pad_x, box_min.y + 10));
    ImGui::PushStyleColor(ImGuiCol_Text, is_lira ? IM_COL32(100,100,100,255) : IM_COL32(180,180,180,255));
    ImGui::Text("%s", is_lira ? "Lira" : "You");
    ImGui::PopStyleColor();

    ImGui::SetCursorScreenPos(ImVec2(box_min.x + pad_x, box_min.y + 30));
    ImGui::PushStyleColor(ImGuiCol_Text, is_lira ? col_lira_text : col_user_text);
    ImGui::PushTextWrapPos(ImGui::GetCursorScreenPos().x + max_w + 1.0f);
    ImGui::TextUnformatted(msg.content.c_str());
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();

    // Reset
    ImGui::SetCursorScreenPos(ImVec2(start.x, start.y + bubble_sz.y));
    ImGui::Dummy(ImVec2(0.0f, 20.0f));
}

void RenderSettingsPopup(float width) {
    if (!app.settings_open) return;
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(400, 320));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(col_sidebar.x+0.05f, col_sidebar.y+0.05f, col_sidebar.z+0.05f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, col_accent);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20,20));

    if (ImGui::Begin("##Settings", &app.settings_open, ImGuiWindowFlags_NoDecoration)) {
        ImGui::TextColored(col_accent, "SYSTEM SETTINGS");
        ImGui::Separator();
        ImGui::Dummy(ImVec2(0, 10));

        ImGui::PushItemWidth(-1);
        ImGui::TextDisabled("Theme");
        const char* themes[] = { "Kawaii", "Lira", "Aria", "Lumina" };
        if (ImGui::Combo("##Theme", &app.current_theme, themes, 4)) SetTheme(app.current_theme);

        ImGui::Dummy(ImVec2(0, 10));
        ImGui::TextDisabled("Model ID");
        ImGui::InputText("##Model", app.model, 128);
        ImGui::Dummy(ImVec2(0, 5));
        ImGui::TextDisabled("API Key");
        ImGui::InputText("##Key", app.apiKey, 512, ImGuiInputTextFlags_Password);
        ImGui::PopItemWidth();

        ImGui::Dummy(ImVec2(0, 20));
        if (ImGui::Button("Close", ImVec2(360, 35))) app.settings_open = false;
    }
    ImGui::End();
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(2);
}

void RenderNewSessionPopup() {
    if (!app.new_session_open) return;
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(350, 180));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, col_sidebar);
    ImGui::PushStyleColor(ImGuiCol_Border, col_accent);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);

    ImGui::OpenPopup("New Session");
    if (ImGui::BeginPopupModal("New Session", nullptr, ImGuiWindowFlags_NoDecoration)) {
        ImGui::Text("Enter Name for new chat:");
        ImGui::Dummy(ImVec2(0, 10));

        ImGui::PushItemWidth(-1);
        bool enter = ImGui::InputText("##Name", app.new_session_buf, 64, ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::PopItemWidth();

        ImGui::Dummy(ImVec2(0, 20));
        if (ImGui::Button("Create", ImVec2(150, 35)) || enter) {
            if (strlen(app.new_session_buf) > 0) {
                app.switch_session(app.new_session_buf);
                app.refresh_sessions();
                app.new_session_open = false;
                app.new_session_buf[0] = '\0';
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(150, 35))) {
            app.new_session_open = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

void RenderSidebarItem(const std::string& label, bool selected) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    float width = ImGui::GetContentRegionAvail().x;
    float height = 35.0f;

    bool hovered = ImGui::IsMouseHoveringRect(p, ImVec2(p.x + width, p.y + height));
    if (hovered || selected) {
        ImVec4 c = selected ? col_accent : col_accent;
        c.w = selected ? 0.2f : 0.1f;
        ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x+width, p.y+height), ImGui::ColorConvertFloat4ToU32(c), 6.0f);
    }

    ImGui::SetCursorScreenPos(ImVec2(p.x+10, p.y+8));
    if (selected) ImGui::PushStyleColor(ImGuiCol_Text, col_accent);
    ImGui::Text("%s", label.c_str());
    if (selected) ImGui::PopStyleColor();

    ImGui::SetCursorScreenPos(p);
    if (ImGui::InvisibleButton(label.c_str(), ImVec2(width, height))) {
        app.switch_session(label);
    }
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + height + 5.0f));
}

void ApplyStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 0.0f; style.ChildRounding = 0.0f; style.FrameRounding = 6.0f;
    style.ItemSpacing = ImVec2(8, 8); style.WindowPadding = ImVec2(0, 0);
    style.ScrollbarRounding = 12.0f; style.ScrollbarSize = 10.0f;
}

int main(int, char**) {
    if (!glfwInit()) return 1;
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    GLFWwindow* window = glfwCreateWindow(1280, 900, "Lira Agent", NULL, NULL);
    if (!window) return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImFontConfig config; config.SizePixels = 15.0f; font_regular = io.Fonts->AddFontDefault(&config);
    ImFontConfig config_lg; config_lg.SizePixels = 24.0f; font_input = io.Fonts->AddFontDefault(&config_lg);

    ApplyStyle();
    SetTheme(1); // Set default theme
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    char input_buffer[4096] = "";

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplGlfw_NewFrame(); ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0,0)); ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("Root", nullptr, ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);

        float sidebar_w = 250.0f;
        float input_h = 120.0f;
        float content_h = ImGui::GetContentRegionAvail().y;
        float content_w = ImGui::GetContentRegionAvail().x;

        // --- Sidebar ---
        ImGui::PushStyleColor(ImGuiCol_ChildBg, col_sidebar);
        ImGui::BeginChild("Sidebar", ImVec2(sidebar_w, content_h));
        {
            ImGui::SetCursorPos(ImVec2(15, 20));
            ImGui::BeginGroup();
            ImGui::PushFont(font_input); ImGui::TextColored(col_accent, "LIRA"); ImGui::PopFont();
            ImGui::Dummy(ImVec2(0, 20));
            if (ImGui::Button(" + New Session ", ImVec2(sidebar_w - 30, 40))) { app.new_session_open = true; }
            ImGui::Dummy(ImVec2(0, 20));
            ImGui::TextDisabled("CHATS");
            ImGui::Dummy(ImVec2(0, 10));
            ImGui::EndGroup();

            ImGui::SetCursorPosX(10);
            ImGui::PushItemWidth(sidebar_w - 20);

            // Render Session List
            for (const auto& session : app.session_list) {
                bool is_active = (session == app.active_agent->current_session_name);
                RenderSidebarItem(session, is_active);
            }
            ImGui::PopItemWidth();

            // Footer
            float foot_h = 70.0f, foot_m = 15.0f;
            ImGui::SetCursorPos(ImVec2(foot_m, content_h - foot_h - foot_m));
            ImVec2 p = ImGui::GetCursorScreenPos();
            float card_w = sidebar_w - (foot_m * 2);

            ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(p.x+3, p.y+3), ImVec2(p.x+card_w+3, p.y+foot_h+3), col_shadow, 10.0f);
            ImU32 foot_bg = ImGui::ColorConvertFloat4ToU32(ImVec4(col_sidebar.x+0.05f, col_sidebar.y+0.05f, col_sidebar.z+0.05f, 1.0f));
            ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x+card_w, p.y+foot_h), foot_bg, 10.0f);

            ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(p.x+25, p.y+35), 18.0f, IM_COL32(220,170,220,255));
            ImGui::GetWindowDrawList()->AddText(ImVec2(p.x+20, p.y+28), IM_COL32(20,20,20,255), "U");

            ImGui::SetCursorPos(ImVec2(foot_m + 55, content_h - foot_h - foot_m + 17));
            ImGui::BeginGroup(); ImGui::Text("User"); ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetColorU32(ImGuiCol_TextDisabled)); ImGui::Text("Pro Plan"); ImGui::PopStyleColor(); ImGui::EndGroup();

            ImGui::SameLine();
            ImGui::SetCursorPos(ImVec2(foot_m + card_w - 40, content_h - foot_h - foot_m + 20));
            if (ImGui::Button("*", ImVec2(30, 30))) app.settings_open = !app.settings_open;
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();

        // Render Popups (Z-Order Top)
        RenderSettingsPopup(sidebar_w);
        RenderNewSessionPopup();

        ImGui::SameLine();

        // --- Main ---
        ImGui::BeginGroup();
        float chat_h = content_h - input_h;
        float chat_w = content_w - sidebar_w;

        ImGui::BeginChild("ChatHistory", ImVec2(chat_w, chat_h));
        {
            ImGui::Dummy(ImVec2(0, 40));
            ImGui::Indent(50);

            // Get History from Agent
            auto history = app.active_agent->get_display_history();
            if (history.empty()) {
                ImGui::TextDisabled("New conversation started. Say hello!");
            }
            for (const auto& msg : history) {
                RenderMessageBubble(msg, chat_w - 100);
            }

            ImGui::Unindent(50);
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();

        ImGui::BeginChild("InputArea", ImVec2(chat_w, input_h));
        {
            float bar_h = 60.0f, btn_w = 70.0f, pad_x = 50.0f;
            ImGui::SetCursorPos(ImVec2(pad_x, (input_h - bar_h) / 2.0f - 10.0f));

            ImGui::PushStyleColor(ImGuiCol_FrameBg, col_input_bg);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(15, 15));
            ImGui::PushFont(font_input);

            bool enter = ImGui::InputTextMultiline("##Input", input_buffer, sizeof(input_buffer), ImVec2(chat_w - (pad_x*2) - btn_w - 15, bar_h), ImGuiInputTextFlags_CtrlEnterForNewLine|ImGuiInputTextFlags_EnterReturnsTrue);

            ImGui::PopFont(); ImGui::PopStyleVar(2); ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::PushFont(font_input);
            if (ImGui::Button(" > ", ImVec2(btn_w, bar_h)) || enter) {
                if (strlen(input_buffer) > 0) {
                    // Update Agent History (Simulated processing for now)
                    std::string prompt = input_buffer;

                    // We manually inject the user message into the agent's history so it appears immediately
                    // Note: In next step (threading), we will call app.active_agent->process(prompt) in a thread.
                    // For now, we update history so the UI refreshes.

                    // Hack: Access underlying json history to push user message for display
                    app.active_agent->history.push_back({{"role", "user"}, {"content", prompt}});

                    // Clear input
                    input_buffer[0] = '\0';
                    ImGui::SetKeyboardFocusHere(-1);
                }
            }
            ImGui::PopFont();
        }
        ImGui::EndChild();
        ImGui::EndGroup();
        ImGui::End();

        ImGui::Render();
        int w, h; glfwGetFramebufferSize(window, &w, &h); glViewport(0,0,w,h);
        glClearColor(col_bg.x, col_bg.y, col_bg.z, col_bg.w); glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext(); glfwDestroyWindow(window); glfwTerminate();
    return 0;
}