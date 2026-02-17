#include <iostream>
#include <vector>
#include <string>
#include <windows.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

std::vector<std::string> history;

// Clipboard eken text ganna hati
std::string GetClipboardText() {
    if (!OpenClipboard(nullptr)) return "";
    HANDLE hData = GetClipboardData(CF_TEXT);
    if (hData == nullptr) { CloseClipboard(); return ""; }
    char* pszText = static_cast<char*>(GlobalLock(hData));
    std::string text(pszText);
    GlobalUnlock(hData);
    CloseClipboard();
    return text;
}

// Ayeth Clipboard ekata danna (Click kalama)
void SetClipboardText(const std::string& text) {
    OpenClipboard(nullptr);
    EmptyClipboard();
    HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
    memcpy(GlobalLock(hGlob), text.c_str(), text.size() + 1);
    GlobalUnlock(hGlob);
    SetClipboardData(CF_TEXT, hGlob);
    CloseClipboard();
}

int main() {
    // Setup Window (GLFW)
    if (!glfwInit()) return 1;
    GLFWwindow* window = glfwCreateWindow(400, 500, "My Clipboard Manager", NULL, NULL);
    glfwMakeContextCurrent(window);
    
    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    std::string lastClip = "";

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        
        // Clipboard monitoring
        std::string currentClip = GetClipboardText();
        if (!currentClip.empty() && currentClip != lastClip) {
            history.insert(history.begin(), currentClip);
            if (history.size() > 20) history.pop_back();
            lastClip = currentClip;
        }

        // Start UI Frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Clipboard History", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
        
        if (ImGui::Button("Clear History")) history.clear();
        ImGui::Separator();

        for (size_t i = 0; i < history.size(); i++) {
            if (ImGui::Selectable(history[i].substr(0, 50).c_str())) {
                SetClipboardText(history[i]);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click to Copy");
        }

        ImGui::End();

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    return 0;
}