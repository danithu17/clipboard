#include <windows.h>
#include <dwmapi.h>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <ctime>

// ImGui & GLFW
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#pragma comment(lib, "dwmapi.lib")

// --- Structure for Items ---
struct ClipItem {
    std::wstring text;
    std::string type;
    std::string time;
    bool isPinned = false;
};

std::vector<ClipItem> history;
std::wstring lastCapturedText = L"";
bool isVisible = true;
bool isDragging = false;
double dragOffsetX, dragOffsetY;
int selectedTab = 0; // 0: All, 1: Links, 2: Code
char searchBuffer[128] = "";

// --- 🪄 WINDOW EFFECTS (Apple Style Glass) ---
void ApplySequoiaGlass(HWND hwnd) {
    BOOL darkMode = TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));
    
    // Windows 11 Acrylic Backdrop
    int backdrop = 3; 
    DwmSetWindowAttribute(hwnd, 38, &backdrop, sizeof(backdrop));

    // Rounded Corners
    int corner = 2; // 2 for Rounded
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
}

// --- 📋 CLIPBOARD LOGIC ---
void UpdateClipboard() {
    if (!OpenClipboard(nullptr)) return;
    if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
        HANDLE hData = GetClipboardData(CF_UNICODETEXT);
        if (hData) {
            wchar_t* pText = (wchar_t*)GlobalLock(hData);
            if (pText && lastCapturedText != pText) {
                lastCapturedText = pText;
                
                // Get Time
                time_t now = time(0); struct tm t; localtime_s(&t, &now); 
                char timeBuf[10]; strftime(timeBuf, sizeof(timeBuf), "%H:%M", &t);
                
                // Type Detect
                std::string type = (lastCapturedText.find(L"http") != std::wstring::npos) ? "Link" : 
                                   (lastCapturedText.find(L"{") != std::wstring::npos) ? "Code" : "Text";
                
                history.insert(history.begin(), { pText, type, timeBuf, false });
            }
            GlobalUnlock(hData);
        }
    }
    CloseClipboard();
}

int main() {
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(780, 550, "AppleClip Sequoia", NULL, NULL);
    if (!window) return 1;

    HWND hwnd = glfwGetWin32Window(window);
    ApplySequoiaGlass(hwnd);
    glfwMakeContextCurrent(window);

    // Hotkey: Alt + Shift + C
    RegisterHotKey(hwnd, 1, MOD_ALT | MOD_SHIFT | MOD_NOREPEAT, 0x43);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    // Styling
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 20.0f;
    style.FrameRounding = 12.0f;
    style.ItemSpacing = ImVec2(0, 10);
    style.WindowPadding = ImVec2(0, 0);

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

        if (!isVisible) { Sleep(30); continue; }

        glfwPollEvents();
        UpdateClipboard();

        // Escape to hide
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) isVisible = false;

        // --- 🖱️ DRAGGING LOGIC ---
        double mx, my; glfwGetCursorPos(window, &mx, &my);
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
            if (!isDragging && my < 60) { isDragging = true; dragOffsetX = mx; dragOffsetY = my; }
            if (isDragging) {
                int wx, wy; glfwGetWindowPos(window, &wx, &wy);
                glfwSetWindowPos(window, wx + (int)mx - (int)dragOffsetX, wy + (int)my - (int)dragOffsetY);
            }
        } else isDragging = false;

        ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplGlfw_NewFrame(); ImGui::NewFrame();

        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::Begin("Main", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground);
        
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 sz = ImGui::GetIO().DisplaySize;

        // --- 🎨 SEQUOIA UI RENDER ---
        // Main Background (Frosted Glass Look)
        dl->AddRectFilled(ImVec2(0, 0), sz, IM_COL32(20, 20, 25, 210), 20.0f);
        dl->AddRect(ImVec2(0, 0), sz, IM_COL32(255, 255, 255, 45), 20.0f, 0, 1.2f); // Thin border

        // Sidebar Glass Area
        dl->AddRectFilled(ImVec2(0, 0), ImVec2(240, sz.y), IM_COL32(255, 255, 255, 12), 20.0f, ImDrawFlags_RoundCornersLeft);

        // Apple Traffic Lights
        dl->AddCircleFilled(ImVec2(30, 30), 6.5f, IM_COL32(255, 69, 58, 255));
        dl->AddCircleFilled(ImVec2(52, 30), 6.5f, IM_COL32(255, 204, 0, 255));
        dl->AddCircleFilled(ImVec2(74, 30), 6.5f, IM_COL32(52, 199, 89, 255));

        // Sidebar Navigation
        ImGui::SetCursorPos(ImVec2(20, 100));
        auto Nav = [&](const char* label, int id) {
            bool active = (selectedTab == id);
            ImVec2 cur = ImGui::GetCursorScreenPos();
            if (active) {
                dl->AddRectFilled(ImVec2(cur.x - 10, cur.y - 5), ImVec2(cur.x + 210, cur.y + 35), IM_COL32(0, 122, 255, 255), 10.0f);
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(200, 200, 200, 255));
            }
            if (ImGui::Selectable(label, active, 0, ImVec2(200, 30))) selectedTab = id;
            ImGui::PopStyleColor();
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10);
        };

        Nav("   All Snippets", 0);
        Nav("   Web Links", 1);
        Nav("   Code Blocks", 2);

        // --- CONTENT AREA ---
        ImGui::SetCursorPos(ImVec2(270, 35));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 20.0f);
        ImGui::SetNextItemWidth(480);
        ImGui::InputTextWithHint("##Search", "Search everything...", searchBuffer, 128);
        ImGui::PopStyleVar();

        ImGui::SetCursorPos(ImVec2(270, 100));
        if (ImGui::BeginChild("ListScroll", ImVec2(490, 430), false, ImGuiWindowFlags_NoBackground)) {
            for (int i = 0; i < (int)history.size(); i++) {
                // Filter Logic
                if (selectedTab == 1 && history[i].type != "Link") continue;
                if (selectedTab == 2 && history[i].type != "Code") continue;

                ImGui::PushID(i);
                ImVec2 p = ImGui::GetCursorScreenPos();
                bool hovered = ImGui::IsMouseHoveringRect(p, ImVec2(p.x + 480, p.y + 75));
                
                // Card Design
                ImU32 cardCol = hovered ? IM_COL32(255, 255, 255, 25) : IM_COL32(255, 255, 255, 12);
                dl->AddRectFilled(p, ImVec2(p.x + 480, p.y + 75), cardCol, 15.0f);
                
                // Content Information
                dl->AddText(ImVec2(p.x + 20, p.y + 15), IM_COL32(0, 122, 255, 255), history[i].type.c_str());
                dl->AddText(ImVec2(p.x + 420, p.y + 15), IM_COL32(150, 150, 150, 200), history[i].time.c_str());
                
                std::string displayTxt = std::string(history[i].text.begin(), history[i].text.end()).substr(0, 60);
                dl->AddText(ImVec2(p.x + 20, p.y + 40), IM_COL32(255, 255, 255, 255), displayTxt.c_str());

                // Interaction
                if (ImGui::InvisibleButton("##CardBtn", ImVec2(480, 75))) {
                    OpenClipboard(nullptr); EmptyClipboard();
                    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, (history[i].text.size() + 1) * sizeof(wchar_t));
                    memcpy(GlobalLock(h), history[i].text.c_str(), (history[i].text.size() + 1) * sizeof(wchar_t));
                    GlobalUnlock(h); SetClipboardData(CF_UNICODETEXT, h); CloseClipboard();
                    isVisible = false; // Hide after selection
                }

                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 85);
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

    UnregisterHotKey(hwnd, 1);
    glfwTerminate();
    return 0;
}

