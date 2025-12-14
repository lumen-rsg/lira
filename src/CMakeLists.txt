cmake_minimum_required(VERSION 3.20)
project(lira VERSION 1.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Dependencies
find_package(CURL REQUIRED)
find_package(nlohmann_json REQUIRED)

# Executable
add_executable(lira
        Agent.cpp
        Agent.h
        Helpers.h
        Nexus.cpp
        Nexus.h
        StreamRenderer.cpp
        StreamRenderer.h
        WebSearcher.cpp
        WebSearcher.h
        main.cpp)

# Linking
target_link_libraries(lira PRIVATE
        CURL::libcurl
        nlohmann_json::nlohmann_json
)

# Compile Options (Enable warnings, optimization)
if(MSVC)
    target_compile_options(lira PRIVATE /W4 /O2)
else()
    target_compile_options(lira PRIVATE -Wall -Wextra -O3 -march=native)
endif()