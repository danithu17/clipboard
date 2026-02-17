cmake_minimum_required(VERSION 3.15)
project(ClipboardManager)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Packages
find_package(imgui CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(GLEW REQUIRED) # Changed this to standard GLEW find

add_executable(ClipboardManager src/main.cpp)

# Link libraries
# Methana GLEW::GLEW kiyala use karanne standard vcpkg target eka
target_link_libraries(ClipboardManager PRIVATE 
    imgui::imgui 
    glfw 
    GLEW::GLEW
    opengl32 
    User32.lib 
    Gdi32.lib 
    Shell32.lib
)

# Build as a Windows app (no console)
set_target_properties(ClipboardManager PROPERTIES WIN32_EXECUTABLE TRUE)