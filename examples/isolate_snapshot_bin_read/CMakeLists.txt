cmake_minimum_required(VERSION 3.10)  # CMake 3.10+ supports C++17 well
project(isolate_snapshot_bin_read LANGUAGES CXX)  # C++ project

# Require C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)  # Fail if C++17 isn't available

# Create the executable
add_executable(IsolateSnapshotBinRead
    src/main.cpp          # Primary source file
    # src/other.cpp      # Add additional files here
)

# Optional: Set compiler warnings (recommended)
if(MSVC)
    target_compile_options(IsolateSnapshotBinRead PRIVATE /W4 /WX)
    target_link_libraries(IsolateSnapshotBinRead PRIVATE "kernel32.lib")
else()
    target_compile_options(IsolateSnapshotBinRead PRIVATE -Wall -Wextra -Werror -pedantic)
endif()