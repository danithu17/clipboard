#include <iostream>
#include <vector>
#include <string>
#include <windows.h>
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
bool isDragging = false;
bool isVisible = true;
double dragOffsetX, dragOffsetY;
int selectedTab = 0;

// Shortcut Registration (ALT + V)
void SetupGlobalShortcut(HWND hwnd) {
    RegisterHotKey(hwnd, 1, MOD_ALT, 0x56); // 0x56 is 'V'
}

std::string GetCurrentTimeString() {
    time_t now = time(0);
    struct tm tstruct; char buf[10];
    localtime_s(&tstruct, &now);
    strftime(buf, sizeof(buf), "%H:%M", &tstruct);
    return buf;
}

std::string WStringToString(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string str(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &str[0], size, NULL, NULL);
    return str;
}

void ApplyModernSequoiaTheme(bool dark) {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 20.0f;
    style.FrameRounding = 12.0f;
    style.WindowPadding = ImVec2(0, 0);
    style.ItemSpacing = ImVec2(0, 0);

    ImVec4* colors = style.Colors;
    if (dark) {
        colors[ImGuiCol_WindowBg] = ImVec4(0.12f, 0.12f, 0.14f, 0.98f);
        colors[ImGuiCol_Text] = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
    } else {
        colors[ImGuiCol_WindowBg] = ImVec4(0.98f, 0.98f, 1.00f, 0.98f);
        colors[ImGuiCol_Text] = ImVec4(0.12f, 0.12f, 0.15f, 1.00f);
    }
}

void UpdateClipboard() {
    if (!OpenClipboard(nullptr)) return;
    if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
        HANDLE hData = GetClipboardData(CF_UNICODETEXT);
        if (hData) {
            wchar_t* pText = (wchar_t*)GlobalLock(hData);
            if (pText) {
                std::wstring currentText(pText);
                GlobalUnlock(hData);
                if (currentText != lastCapturedText && !currentText.empty()) {
                    lastCapturedText = currentText;
                    std::string type = (currentText.find(L"http") != std::string::npos) ? "Link" : 
                                       (currentText.find(L"{") != std::string::npos) ? "Code" : "Text";
                    history.insert(history.begin(), { currentText, GetCurrentTimeString(), type });
                }
            }
        }
    }
    CloseClipboard();
}

int main() {
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_FLOATING, GLFW_TRUE); // Always on Top

    GLFWwindow* window = glfwCreateWindow(650, 500, "AppleClip Pro", NULL, NULL);
    glfwMakeContextCurrent(window);
    HWND hwnd = glfwGetWin32Window(window);
    SetupGlobalShortcut(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ApplyModernSequoiaTheme(isDarkMode);
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version(130)");

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Listen for Global Shortcut (ALT + V)
        MSG msg;
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_HOTKEY) {
                isVisible = !isVisible;
                if (isVisible) { ShowWindow(hwnd, SW_SHOW); SetForegroundWindow(hwnd); }
                else ShowWindow(hwnd, SW_HIDE);
            }
            TranslateMessage(&msg); DispatchMessage(&msg);
        }

        if (!isVisible) { Sleep(50); continue; }

        UpdateClipboard();
        double mx, my; glfwGetCursorPos(window, &mx, &my);
        
        // Window Movement & Traffic Lights
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
            if (!isDragging && my < 60 && mx > 180) { isDragging = true; dragOffsetX = mx; dragOffsetY = my; }
            if (isDragging) {
                int wx, wy; glfwGetWindowPos(window, &wx, &wy);
                glfwSetWindowPos(window, wx + (int)mx - (int)dragOffsetX, wy + (int)my - (int)dragOffsetY);
            }
            if (my > 18 && my < 38) {
                if (mx > 18 && mx < 38) glfwSetWindowShouldClose(window, true); // Close
                if (mx > 38 && mx < 58) { isVisible = false; ShowWindow(hwnd, SW_HIDE); } // Hide (Hide to tray vibe)
            }
        } else isDragging = false;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("AppleUI", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBackground);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImU32 bg = ImGui::GetColorU32(ImGui::GetStyle().Colors[ImGuiCol_WindowBg]);
        dl->AddRectFilled(ImVec2(0, 0), ImGui::GetIO().DisplaySize, bg, 20.0f);

        // Sidebar - Glassmorphism Style
        dl->AddRectFilled(ImVec2(0, 0), ImVec2(180, 500), ImGui::GetColorU32(ImVec4(0,0,0,0.04f)), 20.0f, ImDrawFlags_RoundCornersLeft);

        // Apple Traffic Lights
        dl->AddCircleFilled(ImVec2(28, 28), 6.5f, IM_COL32(255, 69, 58, 255));
        dl->AddCircleFilled(ImVec2(48, 28), 6.5f, IM_COL32(255, 204, 0, 255));
        dl->AddCircleFilled(ImVec2(68, 28), 6.5f, IM_COL32(52, 199, 89, 255));

        // Sidebar Content
        ImGui::SetCursorPos(ImVec2(15, 80));
        auto SidebarTab = [&](const char* label, int id) {
            bool active = (selectedTab == id);
            if (active) dl->AddRectFilled(ImVec2(10, ImGui::GetCursorScreenPos().y - 5), ImVec2(170, ImGui::GetCursorScreenPos().y + 30), ImGui::GetColorU32(ImVec4(0, 0.48f, 1, 0.15f)), 10.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, active ? ImVec4(0, 0.48f, 1, 1) : ImGui::GetStyle().Colors[ImGuiCol_Text]);
            if (ImGui::Selectable(label, active, 0, ImVec2(150, 25))) selectedTab = id;
            ImGui::PopStyleColor();
            ImGui::Spacing(); ImGui::Spacing();
        };
        SidebarTab("   All Clips", 0);
        SidebarTab("   Links", 1);
        SidebarTab("   Code Snippets", 2);

        // Main Content View
        ImGui::SetCursorPos(ImVec2(200, 25));
        ImGui::BeginGroup();
        ImGui::TextDisabled("SEARCH HISTORY");
        ImGui::SetCursorPosX(200);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 25.0f);
        ImGui::SetNextItemWidth(420);
        ImGui::InputTextWithHint("##Search", "Search your clipboard...", searchBuffer, 128);
        ImGui::PopStyleVar();

        ImGui::SetCursorPos(ImVec2(190, 100));
        if (ImGui::BeginChild("Items", ImVec2(440, 380), false, ImGuiWindowFlags_NoBackground)) {
            for (int i = 0; i < (int)history.size(); i++) {
                std::string filter = (selectedTab == 1) ? "Link" : (selectedTab == 2) ? "Code" : "";
                if (!filter.empty() && history[i].type != filter) continue;

                ImGui::PushID(i);
                std::string utf8 = WStringToString(history[i].text);
                ImVec2 cp = ImGui::GetCursorScreenPos();
                
                if (ImGui::InvisibleButton("##c", ImVec2(430, 65))) {
                    OpenClipboard(nullptr); EmptyClipboard();
                    size_t s = (history[i].text.size() + 1) * sizeof(wchar_t);
                    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, s);
                    memcpy(GlobalLock(h), history[i].text.c_str(), s);
                    GlobalUnlock(h); SetClipboardData(CF_UNICODETEXT, h); CloseClipboard();
                }
                bool hov = ImGui::IsItemHovered();
                ImU32 cBg = hov ? ImGui::GetColorU32(ImVec4(0.5f,0.5f,0.5f,0.1f)) : ImGui::GetColorU32(ImVec4(0.5f,0.5f,0.5f,0.03f));
                dl->AddRectFilled(cp, ImVec2(cp.x + 430, cp.y + 65), cBg, 15.0f);

                dl->AddText(ImVec2(cp.x+15, cp.y+15), ImGui::GetColorU32(ImVec4(0, 0.48f, 1, 1)), history[i].type.c_str());
                dl->AddText(ImVec2(cp.x+15, cp.y+38), ImGui::GetColorU32(ImGui::GetStyle().Colors[ImGuiCol_Text]), utf8.substr(0, 50).c_str());
                dl->AddText(ImVec2(cp.x+380, cp.y+15), ImGui::GetColorU32(ImVec4(0.5f,0.5f,0.5f,0.6f)), history[i].time.c_str());

                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10);
                ImGui::PopID();
            }
            ImGui::EndChild();
        }
        ImGui::EndGroup();
        
        ImGui::End();
        ImGui::Render();
        glClearColor(0,0,0,0); glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }
    return 0;
}

