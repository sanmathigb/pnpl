cmake_minimum_required(VERSION 3.10)
project(pnpl VERSION 0.1.0 LANGUAGES CXX)

# C++17 setup
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add llama.cpp as subdirectory - this handles everything automatically
# CMake will only build it when needed (dependency tracking)
message(STATUS "Adding llama.cpp dependency...")
add_subdirectory(third_party/llama.cpp)

# Define common source files
set(COMMON_SOURCES
        src/inference_runner.cpp
        src/inference_monitor.cpp
        src/push_manager.cpp
        src/pop_manager.cpp
)

# Define the main executable (push/pop CLI)
add_executable(pnpl
        src/main.cpp
        ${COMMON_SOURCES}
)

# Define the server executable
add_executable(pnpl_server
        src/server.cpp
        ${COMMON_SOURCES}
)

# Include directories
target_include_directories(pnpl
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/llama.cpp/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/llama.cpp/ggml/include
)

target_include_directories(pnpl_server
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/llama.cpp/include
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/llama.cpp/ggml/include
)

# Threading
find_package(Threads REQUIRED)

# Link against llama.cpp targets - CMake handles all the details!
target_link_libraries(pnpl
        PRIVATE
        llama
        Threads::Threads
)

target_link_libraries(pnpl_server
        PRIVATE
        llama
        Threads::Threads
)

# Create data directories
set(PROJECT_DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/data")
add_custom_target(create_directories ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_DATA_DIR}/input
        COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_DATA_DIR}/output
        COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_DATA_DIR}/models
        COMMENT "Creating data directories"
)

message(STATUS "Data directories: ${PROJECT_DATA_DIR}")

# Install targets
install(TARGETS pnpl pnpl_server DESTINATION bin)