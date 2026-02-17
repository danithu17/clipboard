#include <iostream>
#include <vector>
#include <string>
#include <windows.h>
#include <shlobj.h>
#include <algorithm>
#include <ctime>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

struct ClipItem { std::wstring text; std::string time; std::string type; };
std::vector<ClipItem> history;
char searchBuffer[128] = "";
std::wstring lastCapturedText = L"";
bool isDarkMode = true; 
bool isVisible = true;
bool isDragging = false;
double dragOffsetX, dragOffsetY;
int selectedTab = 0;

void RegisterStartup() {
    TCHAR szPath[MAX_PATH];
    GetModuleFileName(NULL, szPath, MAX_PATH);
    HKEY hKey;
    RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey);
    RegSetValueEx(hKey, "AppleClipPro", 0, REG_SZ, (const BYTE*)szPath, (DWORD)(strlen(szPath) + 1));
    RegCloseKey(hKey);
}

std::string WStringToString(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string str(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &str[0], size, NULL, NULL);
    return str;
}

// --- REFINED APPLE THEME ---
void ApplyAppleTheme(bool dark) {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 22.0f;
    style.FrameRounding = 12.0f;
    style.ScrollbarRounding = 12.0f;
    style.WindowPadding = ImVec2(0, 0);
    style.ItemSpacing = ImVec2(0, 0);

    ImVec4* colors = style.Colors;
    if (dark) {
        colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.12f, 0.98f);
        colors[ImGuiCol_Text] = ImVec4(0.92f, 0.92f, 0.95f, 1.00f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.18f, 0.18f, 0.20f, 0.80f);
    } else {
        colors[ImGuiCol_WindowBg] = ImVec4(0.98f, 0.98f, 1.00f, 0.98f);
        colors[ImGuiCol_Text] = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.92f, 0.92f, 0.94f, 0.80f);
    }
    colors[ImGuiCol_Header] = ImVec4(0.00f, 0.48f, 1.00f, 0.15f); // Subtle blue
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.00f, 0.48f, 1.00f, 0.25f);
}

int main() {
    RegisterStartup();
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(700, 520, "AppleClip Pro", NULL, NULL);
    glfwMakeContextCurrent(window);
    HWND hwnd = glfwGetWin32Window(window);

    // Register Alt + Shift + C
    RegisterHotKey(hwnd, 1, MOD_ALT | MOD_SHIFT | MOD_NOREPEAT, 0x43);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ApplyAppleTheme(isDarkMode);
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    while (!glfwWindowShouldClose(window)) {
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_HOTKEY) {
                isVisible = !isVisible;
                if (isVisible) { ShowWindow(hwnd, SW_SHOW); SetForegroundWindow(hwnd); }
                else ShowWindow(hwnd, SW_HIDE);
            }
            TranslateMessage(&msg); DispatchMessage(&msg);
        }

        glfwPollEvents();
        
        // Background Tracking
        if (OpenClipboard(nullptr)) {
            if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
                HANDLE hData = GetClipboardData(CF_UNICODETEXT);
                if (hData) {
                    wchar_t* pText = (wchar_t*)GlobalLock(hData);
                    if (pText) {
                        std::wstring curText(pText); GlobalUnlock(hData);
                        if (curText != lastCapturedText && !curText.empty()) {
                            lastCapturedText = curText;
                            std::string type = (curText.find(L"http") != std::string::npos) ? "Link" : 
                                               (curText.find(L"{") != std::string::npos) ? "Code" : "Text";
                            history.insert(history.begin(), { curText, "Just Now", type });
                        }
                    }
                }
            }
            CloseClipboard();
        }

        if (!isVisible) { Sleep(30); continue; }

        ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplGlfw_NewFrame(); ImGui::NewFrame();
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize); ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::Begin("Main", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBackground);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImU32 bgCol = ImGui::GetColorU32(ImGui::GetStyle().Colors[ImGuiCol_WindowBg]);
        dl->AddRectFilled(ImVec2(0, 0), ImGui::GetIO().DisplaySize, bgCol, 22.0f);

        // Sidebar Background
        ImU32 sideBg = isDarkMode ? IM_COL32(255,255,255,10) : IM_COL32(0,0,0,15);
        dl->AddRectFilled(ImVec2(0, 0), ImVec2(220, 520), sideBg, 22.0f, ImDrawFlags_RoundCornersLeft);

        // Movement & Close Logic
        double mx, my; glfwGetCursorPos(window, &mx, &my);
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
            if (!isDragging && (my < 60 || mx < 220)) { isDragging = true; dragOffsetX = mx; dragOffsetY = my; }
            if (isDragging) {
                int wx, wy; glfwGetWindowPos(window, &wx, &wy);
                glfwSetWindowPos(window, wx + (int)mx - (int)dragOffsetX, wy + (int)my - (int)dragOffsetY);
            }
            if (my > 15 && my < 40 && mx < 80) { isVisible = false; ShowWindow(hwnd, SW_HIDE); }
        } else isDragging = false;

        // Traffic Lights
        dl->AddCircleFilled(ImVec2(30, 30), 6.5f, IM_COL32(255, 69, 58, 255));
        dl->AddCircleFilled(ImVec2(50, 30), 6.5f, IM_COL32(255, 204, 0, 255));
        dl->AddCircleFilled(ImVec2(70, 30), 6.5f, IM_COL32(52, 199, 89, 255));

        // Sidebar Content
        ImGui::SetCursorPos(ImVec2(15, 80));
        auto Nav = [&](const char* label, int id) {
            bool active = (selectedTab == id);
            ImVec2 p = ImGui::GetCursorScreenPos();
            if (active) {
                dl->AddRectFilled(ImVec2(p.x - 5, p.y - 2), ImVec2(p.x + 195, p.y + 32), ImGui::GetColorU32(ImVec4(0.00f, 0.48f, 1.00f, 0.85f)), 10.0f);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1,1,1,1));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyle().Colors[ImGuiCol_Text]);
            }
            if (ImGui::Selectable(label, active, 0, ImVec2(180, 30))) selectedTab = id;
            ImGui::PopStyleColor();
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8);
        };
        Nav("  All Clips", 0); Nav("  Links", 1); Nav("  Code Snippets", 2);

        // --- THEME TOGGLE (BOTTOM SIDEBAR) ---
        ImGui::SetCursorPos(ImVec2(15, 470));
        if (ImGui::Button(isDarkMode ? "  Light Mode  " : "  Dark Mode  ", ImVec2(190, 35))) {
            isDarkMode = !isDarkMode;
            ApplyAppleTheme(isDarkMode);
        }

        // Main List
        ImGui::SetCursorPos(ImVec2(240, 30));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 25.0f);
        ImGui::SetNextItemWidth(400);
        ImGui::InputTextWithHint("##S", "Search...", searchBuffer, 128);
        ImGui::PopStyleVar();

        ImGui::SetCursorPos(ImVec2(235, 90));
        if (ImGui::BeginChild("Scroll", ImVec2(445, 400), false, ImGuiWindowFlags_NoBackground)) {
            for (int i = 0; i < (int)history.size(); i++) {
                std::string filter = (selectedTab == 1) ? "Link" : (selectedTab == 2) ? "Code" : "";
                if (!filter.empty() && history[i].type != filter) continue;
                
                ImGui::PushID(i);
                std::string utf8 = WStringToString(history[i].text);
                ImVec2 cp = ImGui::GetCursorScreenPos();
                if (ImGui::InvisibleButton("##b", ImVec2(430, 65))) {
                    OpenClipboard(nullptr); EmptyClipboard();
                    size_t s = (history[i].text.size() + 1) * sizeof(wchar_t);
                    HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, s);
                    memcpy(GlobalLock(hg), history[i].text.c_str(), s);
                    GlobalUnlock(hg); SetClipboardData(CF_UNICODETEXT, hg); CloseClipboard();
                }
                bool h = ImGui::IsItemHovered();
                ImU32 card = h ? (isDarkMode ? IM_COL32(255,255,255,30) : IM_COL32(0,0,0,25)) : (isDarkMode ? IM_COL32(255,255,255,15) : IM_COL32(0,0,0,10));
                dl->AddRectFilled(cp, ImVec2(cp.x+430, cp.y+65), card, 15.0f);
                dl->AddText(ImVec2(cp.x+15, cp.y+15), IM_COL32(0, 122, 255, 255), history[i].type.c_str());
                dl->AddText(ImVec2(cp.x+15, cp.y+35), ImGui::GetColorU32(ImGuiCol_Text), utf8.substr(0, 50).c_str());
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 12);
                ImGui::PopID();
            }
            ImGui::EndChild();
        }

        ImGui::End();
        ImGui::Render();
        glClearColor(0,0,0,0); glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }
    return 0;
}

