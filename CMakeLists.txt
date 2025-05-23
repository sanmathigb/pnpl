cmake_minimum_required(VERSION 3.10)
project(pnpl VERSION 0.1.0 LANGUAGES CXX)

# C++17 setup
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Path to llama.cpp
set(LLAMA_CPP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/llama.cpp")
set(LLAMA_INCLUDE_DIR "${LLAMA_CPP_DIR}/include")
set(GGML_INCLUDE_DIR "${LLAMA_CPP_DIR}/ggml/include")

# Define the llama library explicitly based on third_party build
if(APPLE)
    set(LLAMA_LIBRARY "${LLAMA_CPP_DIR}/build/bin/libllama.dylib")
    # Find additional libraries from llama.cpp that we might need
    file(GLOB GGML_LIBRARIES "${LLAMA_CPP_DIR}/build/bin/libggml*.dylib")
else()
    set(LLAMA_LIBRARY "${LLAMA_CPP_DIR}/build/lib/libllama.so")
    # Find additional libraries from llama.cpp that we might need
    file(GLOB GGML_LIBRARIES "${LLAMA_CPP_DIR}/build/lib/libggml*.so")
endif()

# Print found libraries for debugging
message(STATUS "Found LLAMA_LIBRARY: ${LLAMA_LIBRARY}")
message(STATUS "Found GGML_LIBRARIES: ${GGML_LIBRARIES}")

# Verify the library exists
if(NOT EXISTS ${LLAMA_LIBRARY})
    message(FATAL_ERROR "Could not find llama library at ${LLAMA_LIBRARY}. Please build llama.cpp first.")
endif()

# Verify the headers exist
if(NOT EXISTS "${LLAMA_INCLUDE_DIR}/llama.h")
    message(FATAL_ERROR "Could not find llama.h at ${LLAMA_INCLUDE_DIR}. Please check your llama.cpp installation.")
endif()

if(NOT EXISTS "${GGML_INCLUDE_DIR}/ggml.h")
    message(FATAL_ERROR "Could not find ggml.h at ${GGML_INCLUDE_DIR}. Please check your llama.cpp installation.")
endif()

message(STATUS "Using llama.h from: ${LLAMA_INCLUDE_DIR}")
message(STATUS "Using ggml.h from: ${GGML_INCLUDE_DIR}")

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

# Include directories for both executables
target_include_directories(pnpl
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${LLAMA_INCLUDE_DIR}
        ${GGML_INCLUDE_DIR}
)

target_include_directories(pnpl_server
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${LLAMA_INCLUDE_DIR}
        ${GGML_INCLUDE_DIR}
)

# Threading
find_package(Threads REQUIRED)

# Link llama and threads for both executables
target_link_libraries(pnpl
        PRIVATE
        ${LLAMA_LIBRARY}
        ${GGML_LIBRARIES}
        Threads::Threads
)

target_link_libraries(pnpl_server
        PRIVATE
        ${LLAMA_LIBRARY}
        ${GGML_LIBRARIES}
        Threads::Threads
)

# Create data directories OUTSIDE the build folder
# Use the project root directory instead
set(PROJECT_DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/data")
add_custom_target(create_directories ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_DATA_DIR}/input
        COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_DATA_DIR}/output
        COMMAND ${CMAKE_COMMAND} -E make_directory ${PROJECT_DATA_DIR}/models
        COMMENT "Creating data directories outside build folder"
)

# Print the data directory location for user reference
message(STATUS "Data directories will be created at: ${PROJECT_DATA_DIR}")
message(STATUS "  - Input: ${PROJECT_DATA_DIR}/input")
message(STATUS "  - Output: ${PROJECT_DATA_DIR}/output")
message(STATUS "  - Models: ${PROJECT_DATA_DIR}/models")

# (Optional) Install targets
install(TARGETS pnpl pnpl_server DESTINATION bin)