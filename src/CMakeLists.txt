cmake_minimum_required(VERSION 3.20)
project(lira VERSION 1.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Dependencies ---
find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)

# --- GLFW & OpenGL (System) ---
if(APPLE)
    # Fix: Add Homebrew include/lib paths explicitly
    include_directories("/opt/homebrew/include")
    include_directories("/usr/local/include")
    link_directories("/opt/homebrew/lib")
    link_directories("/usr/local/lib")

    # Use find_package for GLFW to get the targets properly
    find_package(glfw3 REQUIRED)

    # macOS Frameworks
    find_library(FRAMEWORK_OPENGL OpenGL)
    find_library(FRAMEWORK_COCOA Cocoa)
    find_library(FRAMEWORK_IOKIT IOKit)
    find_library(FRAMEWORK_COREVIDEO CoreVideo)

    # Link against the glfw target found by find_package
    set(GUI_LIBS glfw ${FRAMEWORK_OPENGL} ${FRAMEWORK_COCOA} ${FRAMEWORK_IOKIT} ${FRAMEWORK_COREVIDEO})
else()
    find_package(OpenGL REQUIRED)
    find_package(glfw3 REQUIRED)
    set(GUI_LIBS OpenGL::GL glfw)
endif()

# --- Fetch ImGui ---
include(FetchContent)
FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui
        GIT_TAG        docking
)
FetchContent_MakeAvailable(imgui)

# --- Shared Logic Library ---
add_library(lira_core
        Agent.cpp
        Nexus.cpp
        StreamRenderer.cpp
        WebSearcher.cpp
)
target_link_libraries(lira_core PRIVATE CURL::libcurl nlohmann_json::nlohmann_json)

# --- CLI Executable ---
add_executable(lira main.cpp)
target_link_libraries(lira PRIVATE lira_core)

# --- GUI Executable ---
add_executable(lira-gui
        Gui.cpp
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(lira-gui PRIVATE
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(lira-gui PRIVATE
        lira_core
        ${GUI_LIBS}
)