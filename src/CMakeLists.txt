cmake_minimum_required(VERSION 3.20)
project(lira VERSION 1.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Force colored output even when piping to logs
set(CMAKE_COLOR_DIAGNOSTICS ON)
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options(-fdiagnostics-color=always)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_compile_options(-fcolor-diagnostics)
endif()

# Dependencies
find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)

# Source Files (Updated for your refactor)
add_executable(lira
        Agent.cpp
        Nexus.cpp
        StreamRenderer.cpp
        WebSearcher.cpp
        main.cpp
)

# Linking
target_link_libraries(lira PRIVATE
        CURL::libcurl
        nlohmann_json::nlohmann_json
)

# Optimization
if(MSVC)
    target_compile_options(lira PRIVATE /W4 /O2)
else()
    target_compile_options(lira PRIVATE -Wall -Wextra -O3 -march=native)
endif()