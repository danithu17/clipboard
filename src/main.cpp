#include <iostream>
#include <vector>
#include <string>
#include <windows.h>
#include <algorithm>
#include <regex>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

enum Category { ALL, TEXT, LINKS, CODE };

struct ClipItem {
    std::wstring text;
    Category cat;
    bool pinned = false;
};

std::vector<ClipItem> history;
char searchBuffer[128] = "";
std::wstring lastCapturedText = L"";

// Unicode to UTF8 for Display
std::string WStringToString(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string str(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &str[0], size, NULL, NULL);
    return str;
}

// Smart Category Detection
Category IdentifyCategory(const std::wstring& wstr) {
    std::string str = WStringToString(wstr);
    if (str.find("http") != std::string::npos || str.find("www.") != std::string::npos) return LINKS;
    if (str.find("{") != std::string::npos || str.find(";") != std::string::npos || str.find("function") != std::string::npos) return CODE;
    return TEXT;
}

// Theme Apply (Cyberpunk Neon)
void ApplyCyberpunkTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 10.0f;
    style.FrameRounding = 6.0f;
    style.ItemSpacing = ImVec2(10, 10);
    style.ScrollbarRounding = 10.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.20f, 0.22f, 0.47f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.30f, 0.35f, 0.70f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.12f, 0.12f, 0.18f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.30f, 0.60f, 1.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.10f, 0.15f, 1.00f);
    colors[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.95f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.20f, 0.20f, 0.25f, 1.00f);
}

void UpdateClipboard() {
    if (!OpenClipboard(nullptr)) return;
    if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
        HANDLE hData = GetClipboardData(CF_UNICODETEXT);
        if (hData) {
            wchar_t* pText = static_cast<wchar_t*>(GlobalLock(hData));
            if (pText) {
                std::wstring currentText(pText);
                GlobalUnlock(hData);
                if (currentText != lastCapturedText && !currentText.empty()) {
                    lastCapturedText = currentText;
                    history.insert(history.begin(), { currentText, IdentifyCategory(currentText), false });
                    if (history.size() > 100) history.pop_back();
                }
            }
        }
    }
    CloseClipboard();
}

int main() {
    if (!glfwInit()) return 1;
    GLFWwindow* window = glfwCreateWindow(420, 650, "Dynamo Clipboard Pro", NULL, NULL);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // Always on Top
    HWND hwnd = glfwGetWin32Window(window);
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ApplyCyberpunkTheme();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    Category currentTab = ALL;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        UpdateClipboard();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("Dashboard", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);

        // Neon Header
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.7f, 1.0f), ">> DYNAMO CLIPBOARD PRO");
        ImGui::Separator();

        // CATEGORY TABS WITH NEON COLORS
        auto TabButton = [&](const char* label, Category cat, ImVec4 activeColor) {
            if (currentTab == cat) ImGui::PushStyleColor(ImGuiCol_Button, activeColor);
            if (ImGui::Button(label, ImVec2(90, 30))) currentTab = cat;
            if (currentTab == cat) ImGui::PopStyleColor();
        };

        TabButton("All", ALL, ImVec4(0.2f, 0.4f, 0.8f, 1.0f)); ImGui::SameLine();
        TabButton("Links", LINKS, ImVec4(0.0f, 0.7f, 0.9f, 1.0f)); ImGui::SameLine();
        TabButton("Code", CODE, ImVec4(0.7f, 0.0f, 0.9f, 1.0f)); ImGui::SameLine();
        TabButton("Text", TEXT, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));

        ImGui::Spacing();
        ImGui::InputTextWithHint("##Search", "Quick search history...", searchBuffer, 128);
        ImGui::Separator();

        if (ImGui::BeginChild("ItemsList", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
            for (int i = 0; i < (int)history.size(); i++) {
                if (currentTab != ALL && history[i].cat != currentTab) continue;

                std::string utf8 = WStringToString(history[i].text);
                std::string search(searchBuffer);
                if (!search.empty()) {
                    std::string low = utf8; std::transform(low.begin(), low.end(), low.begin(), ::tolower);
                    std::transform(search.begin(), search.end(), search.begin(), ::tolower);
                    if (low.find(search) == std::string::npos) continue;
                }

                ImGui::PushID(i);
                
                // Color Tag Indicator
                ImVec4 tagColor = (history[i].cat == LINKS) ? ImVec4(0, 0.7, 1, 1) : (history[i].cat == CODE) ? ImVec4(0.7, 0, 1, 1) : ImVec4(0.5, 0.5, 0.5, 1);
                ImGui::TextColored(tagColor, "|"); ImGui::SameLine();

                if (history[i].pinned) { ImGui::TextColored(ImVec4(1, 0.9, 0, 1), "[PIN]"); ImGui::SameLine(); }

                std::string preview = utf8.substr(0, 40) + (utf8.size() > 40 ? "..." : "");
                if (ImGui::Selectable(preview.c_str(), false, 0, ImVec2(0, 45))) {
                    if (OpenClipboard(nullptr)) {
                        EmptyClipboard();
                        size_t s = (history[i].text.size() + 1) * sizeof(wchar_t);
                        HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, s);
                        memcpy(GlobalLock(h), history[i].text.c_str(), s);
                        GlobalUnlock(h); SetClipboardData(CF_UNICODETEXT, h); CloseClipboard();
                        lastCapturedText = history[i].text;
                    }
                }
                
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("Double-click to view full or Click to copy");
                    style.Colors[ImGuiCol_Text] = ImVec4(0, 1, 0.8, 1); // Hover effect
                } else {
                    style.Colors[ImGuiCol_Text] = ImVec4(0.9, 0.9, 0.95, 1);
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
        Sleep(50);
    }
    return 0;
}