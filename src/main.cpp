#include <iostream>
#include <vector>
#include <string>
#include <windows.h>
#include <algorithm>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

struct ClipItem {
    std::string text;
    bool isImage = false;
    bool pinned = false;
};

std::vector<ClipItem> history;
char searchBuffer[128] = "";

// Windows API use karala window eka uda thiyaganna (Always on Top)
void SetAlwaysOnTop(GLFWwindow* window) {
    HWND hwnd = glfwGetWin32Window(window);
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

// Clipboard eka check karala text/images detect kireema
void UpdateClipboardLogic() {
    if (!OpenClipboard(nullptr)) return;

    if (IsClipboardFormatAvailable(CF_UNICODETEXT) || IsClipboardFormatAvailable(CF_TEXT)) {
        HANDLE hData = GetClipboardData(CF_TEXT);
        if (hData) {
            char* pszText = static_cast<char*>(GlobalLock(hData));
            if (pszText) {
                std::string text(pszText);
                GlobalUnlock(hData);

                // Duplicate check
                auto it = std::find_if(history.begin(), history.end(), [&](const ClipItem& item) {
                    return item.text == text;
                });

                if (it == history.end() && !text.empty()) {
                    history.insert(history.begin(), {text, false, false});
                    if (history.size() > 100) history.pop_back();
                }
            }
        }
    } 
    else if (IsClipboardFormatAvailable(CF_BITMAP)) {
        std::string imgTag = "[IMAGE DATA - Copy detected]";
        auto it = std::find_if(history.begin(), history.end(), [&](const ClipItem& item) {
            return item.text == imgTag;
        });
        if (it == history.end()) {
            history.insert(history.begin(), {imgTag, true, false});
        }
    }
    CloseClipboard();
}

int main() {
    if (!glfwInit()) return 1;
    
    // Modern small window
    GLFWwindow* window = glfwCreateWindow(400, 600, "Danithu Clipboard Pro", NULL, NULL);
    if (!window) return 1;
    
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    SetAlwaysOnTop(window); 

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiStyle& style = ImGui::GetStyle();
    
    // Modern Dark Theme Styling
    style.WindowRounding = 10.0f;
    style.FrameRounding = 5.0f;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.20f, 0.25f, 0.30f, 1.00f);
    style.Colors[ImGuiCol_Button] = ImVec4(0.25f, 0.30f, 0.50f, 1.00f);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        UpdateClipboardLogic();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("Clipboard", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);

        // Header
        ImGui::TextColored(ImVec4(0.0f, 0.9f, 1.0f, 1.0f), "CLIPBOARD PRO");
        ImGui::SameLine(ImGui::GetWindowWidth() - 70);
        if(ImGui::SmallButton("Clear")) history.clear();
        ImGui::Separator();

        // Search Bar
        ImGui::InputTextWithHint("##Search", "Search clips...", searchBuffer, 128);
        ImGui::Spacing();

        if (ImGui::BeginChild("ListArea")) {
            std::string searchStr(searchBuffer);
            std::transform(searchStr.begin(), searchStr.end(), searchStr.begin(), ::tolower);

            for (int i = 0; i < history.size(); i++) {
                std::string lowerText = history[i].text;
                std::transform(lowerText.begin(), lowerText.end(), lowerText.begin(), ::tolower);

                if (!searchStr.empty() && lowerText.find(searchStr) == std::string::npos) continue;

                ImGui::PushID(i);
                
                // Color coding for Pinned and Images
                if (history[i].pinned) ImGui::TextColored(ImVec4(1, 0.8f, 0, 1), "[PINNED]");
                ImVec4 textColor = history[i].isImage ? ImVec4(1, 0.4f, 0.4f, 1) : ImVec4(0.9f, 0.9f, 0.9f, 1);
                
                std::string display = history[i].text.substr(0, 60);
                if (ImGui::Selectable(display.c_str(), false, 0, ImVec2(0, 35))) {
                    if (!history[i].isImage) {
                        // Restore text to clipboard
                        OpenClipboard(nullptr);
                        EmptyClipboard();
                        HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, history[i].text.size() + 1);
                        memcpy(GlobalLock(hGlob), history[i].text.c_str(), history[i].text.size() + 1);
                        GlobalUnlock(hGlob);
                        SetClipboardData(CF_TEXT, hGlob);
                        CloseClipboard();
                    }
                }

                // Right click options
                if (ImGui::BeginPopupContextItem()) {
                    if (ImGui::MenuItem("Pin/Unpin")) {
                        history[i].pinned = !history[i].pinned;
                        std::sort(history.begin(), history.end(), [](const ClipItem& a, const ClipItem& b){
                            return a.pinned > b.pinned;
                        });
                    }
                    if (ImGui::MenuItem("Delete")) {
                        history.erase(history.begin() + i);
                        ImGui::EndPopup(); ImGui::PopID(); break;
                    }
                    ImGui::EndPopup();
                }
                ImGui::Separator();
                ImGui::PopID();
            }
            ImGui::EndChild();
        }
        ImGui::End();

        ImGui::Render();
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}