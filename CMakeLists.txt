cmake_minimum_required(VERSION 3.10)
project(ClipboardManager)

set(CMAKE_CXX_STANDARD 17)

# Find packages
find_package(imgui CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glew CONFIG REQUIRED)

add_executable(ClipboardManager src/main.cpp)

# Link libraries
target_link_libraries(ClipboardManager PRIVATE imgui::imgui glfw glew::glew opengl32)