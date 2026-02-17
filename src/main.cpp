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
    std::wstring text; // Unicode support (Sinhala/Emoji)
    bool isImage = false;
    bool pinned = false;
};

std::vector<ClipItem> history;
char searchBuffer[128] = "";
std::wstring lastCapturedText = L"";

// Wstring to String conversion for ImGui display
std::string WStringToString(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

// Clipboard Update Logic (Dynamic Fix)
void UpdateClipboardDynamic() {
    // Clipboard eka wena app ekakin open karala thibboth 10ms inna
    if (!OpenClipboard(nullptr)) return;

    // Unicode Text detect kireema (Standard text walatath wada karanawa)
    if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
        HANDLE hData = GetClipboardData(CF_UNICODETEXT);
        if (hData) {
            wchar_t* pText = static_cast<wchar_t*>(GlobalLock(hData));
            if (pText) {
                std::wstring currentText(pText);
                GlobalUnlock(hData);

                if (currentText != lastCapturedText && !currentText.empty()) {
                    lastCapturedText = currentText;

                    // Duplicate check
                    auto it = std::find_if(history.begin(), history.end(), [&](const ClipItem& item) {
                        return item.text == currentText;
                    });

                    if (it == history.end()) {
                        history.insert(history.begin(), { currentText, false, false });
                        if (history.size() > 100) history.pop_back();
                    }
                }
            }
        }
    }
    else if (IsClipboardFormatAvailable(CF_BITMAP)) {
        std::wstring imgTag = L"[IMAGE DATA DETECTED]";
        if (lastCapturedText != imgTag) {
            history.insert(history.begin(), { imgTag, true, false });
            lastCapturedText = imgTag;
        }
    }
    CloseClipboard();
}

int main() {
    if (!glfwInit()) return 1;

    GLFWwindow* window = glfwCreateWindow(420, 600, "Danithu Clipboard Pro", NULL, NULL);
    if (!window) return 1;

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // Always on Top
    HWND hwnd = glfwGetWin32Window(window);
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 10.0f;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        
        // Background check (Dynamic update)
        UpdateClipboardDynamic();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("Main", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);

        ImGui::TextColored(ImVec4(0, 1, 0.8f, 1), "DYNAMO CLIPBOARD PRO");
        ImGui::Separator();
        ImGui::InputTextWithHint("##Search", "Search...", searchBuffer, 128);

        if (ImGui::BeginChild("Items")) {
            for (int i = 0; i < (int)history.size(); i++) {
                std::string utf8Text = WStringToString(history[i].text);
                
                // Search filter
                std::string searchStr(searchBuffer);
                if (!searchStr.empty()) {
                    std::string lowerText = utf8Text;
                    std::transform(lowerText.begin(), lowerText.end(), lowerText.begin(), ::tolower);
                    std::transform(searchStr.begin(), searchStr.end(), searchStr.begin(), ::tolower);
                    if (lowerText.find(searchStr) == std::string::npos) continue;
                }

                ImGui::PushID(i);
                if (history[i].pinned) ImGui::TextColored(ImVec4(1, 0.9f, 0, 1), "[PIN]");
                
                if (ImGui::Selectable(utf8Text.substr(0, 50).c_str(), false, 0, ImVec2(0, 35))) {
                    if (!history[i].isImage) {
                        // Copy back to clipboard
                        if (OpenClipboard(nullptr)) {
                            EmptyClipboard();
                            size_t size = (history[i].text.size() + 1) * sizeof(wchar_t);
                            HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, size);
                            memcpy(GlobalLock(hGlob), history[i].text.c_str(), size);
                            GlobalUnlock(hGlob);
                            SetClipboardData(CF_UNICODETEXT, hGlob);
                            CloseClipboard();
                            lastCapturedText = history[i].text; // Update lastCaptured so it doesn't re-add
                        }
                    }
                }
                
                if (ImGui::BeginPopupContextItem()) {
                    if (ImGui::MenuItem("Pin/Unpin")) {
                        history[i].pinned = !history[i].pinned;
                        std::sort(history.begin(), history.end(), [](const ClipItem& a, const ClipItem& b){
                            return a.pinned > b.pinned;
                        });
                    }
                    if (ImGui::MenuItem("Delete")) { history.erase(history.begin() + i); ImGui::EndPopup(); ImGui::PopID(); break; }
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
        
        Sleep(100); // 100ms delay ekak CPU eka save karanna
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}