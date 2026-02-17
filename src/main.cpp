#include <windows.h>
#include <dwmapi.h>
#include <gdiplus.h>
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

// --- 🛠️ AUTO-LINK LIBRARIES ---
// මේ පේළි ටික නිසා තමයි උඹට වෙනම settings හදන්න ඕනේ නැත්තේ
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "opengl32.lib")

using namespace Gdiplus;

// --- Data Structure ---
struct ClipItem {
    std::wstring text;
    std::string type;
    std::string time;
    bool isPinned = false;
    bool hasImage = false;
};

std::vector<ClipItem> history;
std::wstring lastCapturedText = L"";
bool isVisible = true;
bool isDragging = false;
double dragOffsetX, dragOffsetY;
int selectedTab = 0;
char searchBuffer[128] = "";

// --- 🪄 Apple Sequoia Glass Effect Logic ---
void ApplySequoiaDesign(HWND hwnd) {
    BOOL darkMode = TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));
    
    // Windows 11 Acrylic Backdrop (Thick Glass)
    int backdrop = 3; 
    DwmSetWindowAttribute(hwnd, 38, &backdrop, sizeof(backdrop));

    // Rounded Corners (Sequoia Style)
    int corner = 2; 
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
}

// --- 📋 Clip Monitor Logic ---
void MonitorClipboard() {
    if (!OpenClipboard(nullptr)) return;

    if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
        HANDLE hData = GetClipboardData(CF_UNICODETEXT);
        if (hData) {
            wchar_t* pText = (wchar_t*)GlobalLock(hData);
            if (pText && lastCapturedText != pText) {
                lastCapturedText = pText;
                time_t now = time(0); struct tm t; localtime_s(&t, &now); 
                char timeBuf[10]; strftime(timeBuf, sizeof(timeBuf), "%H:%M", &t);
                
                std::string type = "Text";
                if (lastCapturedText.find(L"http") != std::wstring::npos) type = "Link";
                else if (lastCapturedText.find(L"{") != std::wstring::npos) type = "Code";

                history.insert(history.begin(), { pText, type, timeBuf, false, false });
            }
            GlobalUnlock(hData);
        }
    }
    
    if (IsClipboardFormatAvailable(CF_DIB)) {
        if (lastCapturedText != L"__IMAGE_CAP__") {
            lastCapturedText = L"__IMAGE_CAP__";
            time_t now = time(0); struct tm t; localtime_s(&t, &now); 
            char timeBuf[10]; strftime(timeBuf, sizeof(timeBuf), "%H:%M", &t);
            history.insert(history.begin(), { L"Captured Image", "Image", timeBuf, false, true });
        }
    }
    CloseClipboard();
}

int main() {
    // Start GDI+
    GdiplusStartupInput gsi;
    ULONG_PTR gst;
    GdiplusStartup(&gst, &gsi, NULL);

    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "AppleClip Ultra Pro", NULL, NULL);
    HWND hwnd = glfwGetWin32Window(window);
    ApplySequoiaDesign(hwnd);
    glfwMakeContextCurrent(window);

    // Hotkey: Alt + Shift + C
    RegisterHotKey(hwnd, 1, MOD_ALT | MOD_SHIFT, 0x43);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 25.0f;
    style.ChildRounding = 15.0f;
    style.FrameRounding = 12.0f;
    style.WindowPadding = ImVec2(0, 0);

    while (!glfwWindowShouldClose(window)) {
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_HOTKEY) isVisible = !isVisible;
            TranslateMessage(&msg); DispatchMessage(&msg);
        }

        if (isVisible) ShowWindow(hwnd, SW_SHOW); 
        else { ShowWindow(hwnd, SW_HIDE); Sleep(50); continue; }

        glfwPollEvents();
        MonitorClipboard();

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) isVisible = false;

        // --- 🖱️ SMOOTH DRAGGING (TOP 70PX) ---
        double mx, my; glfwGetCursorPos(window, &mx, &my);
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
            if (!isDragging && my < 70) { isDragging = true; dragOffsetX = mx; dragOffsetY = my; }
            if (isDragging) {
                int wx, wy; glfwGetWindowPos(window, &wx, &wy);
                glfwSetWindowPos(window, wx + (int)mx - (int)dragOffsetX, wy + (int)my - (int)dragOffsetY);
            }
        } else isDragging = false;

        ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplGlfw_NewFrame(); ImGui::NewFrame();

        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::Begin("Canvas", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground);
        
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 sz = ImGui::GetIO().DisplaySize;

        // --- 🎨 PREMIUM SEQUOIA UI ---
        // Thick Frosted Glass
        dl->AddRectFilled(ImVec2(0, 0), sz, IM_COL32(20, 20, 25, 235), 25.0f);
        dl->AddRect(ImVec2(0, 0), sz, IM_COL32(255, 255, 255, 40), 25.0f, 0, 1.2f);

        // Sidebar Area
        dl->AddRectFilled(ImVec2(0, 0), ImVec2(240, sz.y), IM_COL32(255, 255, 255, 12), 25.0f, ImDrawFlags_RoundCornersLeft);

        // Traffic Lights
        dl->AddCircleFilled(ImVec2(30, 30), 7, IM_COL32(255, 69, 58, 255));
        dl->AddCircleFilled(ImVec2(55, 30), 7, IM_COL32(255, 204, 0, 255));
        dl->AddCircleFilled(ImVec2(80, 30), 7, IM_COL32(52, 199, 89, 255));

        // Sidebar Nav
        ImGui::SetCursorPos(ImVec2(20, 100));
        auto Nav = [&](const char* label, int id) {
            bool active = (selectedTab == id);
            ImVec2 p = ImGui::GetCursorScreenPos();
            if (active) dl->AddRectFilled(ImVec2(p.x - 10, p.y - 5), ImVec2(p.x + 200, p.y + 35), IM_COL32(0, 122, 255, 255), 12.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, active ? IM_COL32(255, 255, 255, 255) : IM_COL32(200, 200, 200, 255));
            if (ImGui::Selectable(label, active, 0, ImVec2(180, 30))) selectedTab = id;
            ImGui::PopStyleColor();
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10);
        };
        Nav("   All Snippets", 0);
        Nav("   Links", 1);
        Nav("   Images", 2);
        Nav("   Pinned", 3);

        // Content
        ImGui::SetCursorPos(ImVec2(270, 35));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 25.0f);
        ImGui::SetNextItemWidth(480);
        ImGui::InputTextWithHint("##S", "Search history...", searchBuffer, 128);
        ImGui::PopStyleVar();

        ImGui::SetCursorPos(ImVec2(270, 95));
        if (ImGui::BeginChild("List", ImVec2(500, 480), false, ImGuiWindowFlags_NoBackground)) {
            for (int i = 0; i < (int)history.size(); i++) {
                if (selectedTab == 1 && history[i].type != "Link") continue;
                if (selectedTab == 2 && history[i].type != "Image") continue;
                if (selectedTab == 3 && !history[i].isPinned) continue;

                ImGui::PushID(i);
                ImVec2 p = ImGui::GetCursorScreenPos();
                bool hov = ImGui::IsMouseHoveringRect(p, ImVec2(p.x + 480, p.y + 80));

                dl->AddRectFilled(p, ImVec2(p.x + 480, p.y + 80), hov ? IM_COL32(255, 255, 255, 25) : IM_COL32(255, 255, 255, 15), 18.0f);
                dl->AddText(ImVec2(p.x + 20, p.y + 15), IM_COL32(0, 122, 255, 255), history[i].type.c_str());
                
                std::string txt = history[i].hasImage ? "[ Image Data ]" : std::string(history[i].text.begin(), history[i].text.end()).substr(0, 50);
                dl->AddText(ImVec2(p.x + 20, p.y + 40), IM_COL32(255, 255, 255, 255), txt.c_str());

                // Pin Button
                ImGui::SetCursorPos(ImVec2(415, ImGui::GetCursorPos().y + 10));
                if (ImGui::Button(history[i].isPinned ? "Unpin" : "Pin")) history[i].isPinned = !history[i].isPinned;

                // Copy Action
                ImGui::SetCursorScreenPos(p);
                if (ImGui::InvisibleButton("##C", ImVec2(410, 80))) {
                    if (!history[i].hasImage) {
                        OpenClipboard(nullptr); EmptyClipboard();
                        HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, (history[i].text.size() + 1) * sizeof(wchar_t));
                        memcpy(GlobalLock(h), history[i].text.c_str(), (history[i].text.size() + 1) * sizeof(wchar_t));
                        GlobalUnlock(h); SetClipboardData(CF_UNICODETEXT, h); CloseClipboard();
                        isVisible = false;
                    }
                }
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 90);
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

    GdiplusShutdown(gst);
    return 0;
}