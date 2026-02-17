#include <iostream>
#include <vector>
#include <string>
#include <windows.h>
#include <algorithm>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>

struct ClipItem {
    std::string text;
    bool pinned = false;
};

std::vector<ClipItem> history;
char searchBuffer[128] = "";

// Clipboard functions (Issella wage mawa wenas na)
std::string GetClipboardText() {
    if (!OpenClipboard(nullptr)) return "";
    HANDLE hData = GetClipboardData(CF_TEXT);
    if (hData == nullptr) { CloseClipboard(); return ""; }
    char* pszText = static_cast<char*>(GlobalLock(hData));
    std::string text(pszText ? pszText : "");
    GlobalUnlock(hData);
    CloseClipboard();
    return text;
}

void SetClipboardText(const std::string& text) {
    if (!OpenClipboard(nullptr)) return;
    EmptyClipboard();
    HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
    if (hGlob) {
        memcpy(GlobalLock(hGlob), text.c_str(), text.size() + 1);
        GlobalUnlock(hGlob);
        SetClipboardData(CF_TEXT, hGlob);
    }
    CloseClipboard();
}

int main() {
    if (!glfwInit()) return 1;
    GLFWwindow* window = glfwCreateWindow(450, 600, "GenZ Clipboard Pro", NULL, NULL);
    if (!window) return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    
    // Custom Style - Dark & Round Corners
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 8.0f;
    style.FrameRounding = 5.0f;
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.2f, 0.2f, 0.5f, 1.0f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.3f, 0.3f, 0.7f, 1.0f);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    std::string lastClip = "";

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // New Clip Detection
        std::string currentClip = GetClipboardText();
        if (!currentClip.empty() && currentClip != lastClip) {
            bool exists = false;
            for(auto& item : history) if(item.text == currentClip) exists = true;
            if(!exists) {
                history.insert(history.begin(), {currentClip, false});
                if (history.size() > 100) history.pop_back();
            }
            lastClip = currentClip;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("Pro Manager", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);

        // Header
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.8f, 1.0f), "GENZ CLIPBOARD PRO");
        ImGui::SameLine(ImGui::GetWindowWidth() - 70);
        if(ImGui::SmallButton("Clear")) history.clear();
        
        ImGui::Separator();

        // Search Bar
        ImGui::InputTextWithHint("##Search", "Search snippets...", searchBuffer, IM_ARRAYSIZE(searchBuffer));
        ImGui::Spacing();

        if (ImGui::BeginChild("ScrollArea")) {
            std::string searchStr(searchBuffer);
            std::transform(searchStr.begin(), searchStr.end(), searchStr.begin(), ::tolower);

            for (size_t i = 0; i < history.size(); i++) {
                std::string lowerText = history[i].text;
                std::transform(lowerText.begin(), lowerText.end(), lowerText.begin(), ::tolower);

                // Filter search
                if (!searchStr.empty() && lowerText.find(searchStr) == std::string::npos) continue;

                // UI Card for each clip
                ImGui::PushID(i);
                if (history[i].pinned) ImGui::TextColored(ImVec4(1, 1, 0, 1), "[PINNED]");
                
                std::string displayLabel = history[i].text.substr(0, 50) + (history[i].text.size() > 50 ? "..." : "");
                if (ImGui::Selectable(displayLabel.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick)) {
                    SetClipboardText(history[i].text);
                }
                
                // Right click menu
                if (ImGui::BeginPopupContextItem()) {
                    if (ImGui::MenuItem(history[i].pinned ? "Unpin" : "Pin")) {
                        history[i].pinned = !history[i].pinned;
                        // Sort: Pinned items go to top
                        std::sort(history.begin(), history.end(), [](const ClipItem& a, const ClipItem& b) {
                            return a.pinned > b.pinned;
                        });
                    }
                    if (ImGui::MenuItem("Delete")) {
                        history.erase(history.begin() + i);
                        ImGui::EndPopup();
                        ImGui::PopID();
                        break; 
                    }
                    ImGui::EndPopup();
                }
                ImGui::Separator();
                ImGui::PopID();
            }
            ImGui::EndChild();
        }

        ImGui::End();

        // Standard Rendering
        ImGui::Render();
        int dw, dh; glfwGetFramebufferSize(window, &dw, &dh);
        glViewport(0, 0, dw, dh);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}