cmake_minimum_required(VERSION 3.15)
project(ClipboardManager)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(imgui CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(GLEW REQUIRED)

add_executable(ClipboardManager src/main.cpp)

target_link_libraries(ClipboardManager PRIVATE 
    imgui::imgui 
    glfw 
    GLEW::GLEW
    opengl32 
    User32.lib 
    Gdi32.lib 
    Shell32.lib
)

# ME TIKA THAMAI FIX EKA:
if(MSVC)
    # Console eka nathuwa run wenna, eth standard main() eka use karanna me link flags ona
    set_target_properties(ClipboardManager PROPERTIES 
        WIN32_EXECUTABLE TRUE
        LINK_FLAGS "/ENTRY:mainCRTStartup"
    )
endif()