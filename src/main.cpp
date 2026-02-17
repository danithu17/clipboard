#include <windows.h>
#include <dwmapi.h>
#include <gdiplus.h>
#include <shlobj.h>
#include <iostream>
#include <vector>
#include <string>
#include <ctime>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shell32.lib")

using namespace Gdiplus;

// --- Clip Item Structure ---
struct ClipItem {
    std::wstring text; 
    std::string type;  
    std::string time;
    bool isPinned = false;
    Gdiplus::Bitmap* imagePreview = nullptr; // For actual image display
};

std::vector<ClipItem> history;
std::wstring lastCapturedText = L"";
bool isVisible = true;
bool isDraggingWindow = false;
double dragOffsetX, dragOffsetY;
int selectedTab = 0;

// --- 🖼️ IMAGE HELPER: Capture from Clipboard ---
Bitmap* GetImageFromClipboard() {
    if (!OpenClipboard(NULL)) return nullptr;
    HBITMAP hBitmap = (HBITMAP)GetClipboardData(CF_BITMAP);
    if (!hBitmap) { CloseClipboard(); return nullptr; }
    Bitmap* bmp = new Bitmap(hBitmap, (HPALETTE)NULL);
    CloseClipboard();
    return bmp;
}

void ApplySequoiaDesign(HWND hwnd) {
    BOOL darkMode = TRUE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &darkMode, sizeof(darkMode));
    int backdrop = 3; DwmSetWindowAttribute(hwnd, 38, &backdrop, sizeof(backdrop));
    int corner = 2; DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
}

// --- 📥 DRAG & DROP CALLBACK ---
void OnFileDrop(GLFWwindow* window, int count, const char** paths) {
    time_t now = time(0); struct tm t; localtime_s(&t, &now); 
    char timeBuf[10]; strftime(timeBuf, sizeof(timeBuf), "%H:%M", &t);
    
    for (int i = 0; i < count; i++) {
        std::string p = paths[i];
        std::wstring wp(p.begin(), p.end());
        history.insert(history.begin(), { wp, "File", timeBuf, false, nullptr });
    }
}

void MonitorClipboard() {
    if (!OpenClipboard(nullptr)) return;
    time_t now = time(0); struct tm t; localtime_s(&t, &now); 
    char timeBuf[10]; strftime(timeBuf, sizeof(timeBuf), "%H:%M", &t);

    if (IsClipboardFormatAvailable(CF_HDROP)) {
        HDROP hDrop = (HDROP)GetClipboardData(CF_HDROP);
        if (hDrop) {
            wchar_t filePath[MAX_PATH]; DragQueryFileW(hDrop, 0, filePath, MAX_PATH);
            if (lastCapturedText != filePath) {
                lastCapturedText = filePath;
                history.insert(history.begin(), { filePath, "File", timeBuf, false, nullptr });
            }
        }
    } else if (IsClipboardFormatAvailable(CF_BITMAP)) {
        if (lastCapturedText != L"__IMG__") {
            lastCapturedText = L"__IMG__";
            history.insert(history.begin(), { L"Captured Image", "Image", timeBuf, false, GetImageFromClipboard() });
        }
    } else if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
        HANDLE hData = GetClipboardData(CF_UNICODETEXT);
        if (hData) {
            wchar_t* pText = (wchar_t*)GlobalLock(hData);
            if (pText && lastCapturedText != pText) {
                lastCapturedText = pText;
                std::string type = (lastCapturedText.find(L"http") != std::wstring::npos) ? "Link" : "Text";
                history.insert(history.begin(), { pText, type, timeBuf, false, nullptr });
            }
            GlobalUnlock(hData);
        }
    }
    CloseClipboard();
}

int main() {
    GdiplusStartupInput gsi; ULONG_PTR gst; GdiplusStartup(&gst, &gsi, NULL);
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(850, 650, "Apple Sequoia Dropzone", NULL, NULL);
    HWND hwnd = glfwGetWin32Window(window);
    ApplySequoiaDesign(hwnd);
    glfwMakeContextCurrent(window);
    
    // Set Drag & Drop Callback
    glfwSetDropCallback(window, OnFileDrop);
    RegisterHotKey(hwnd, 1, MOD_ALT | MOD_SHIFT, 0x43);

    IMGUI_CHECKVERSION(); ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 28.0f; style.FrameRounding = 14.0f;

    while (!glfwWindowShouldClose(window)) {
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_HOTKEY) isVisible = !isVisible;
            TranslateMessage(&msg); DispatchMessage(&msg);
        }
        if (isVisible) ShowWindow(hwnd, SW_SHOW); 
        else { ShowWindow(hwnd, SW_HIDE); Sleep(50); continue; }

        glfwPollEvents(); MonitorClipboard();
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) isVisible = false;

        // Window Dragging
        double mx, my; glfwGetCursorPos(window, &mx, &my);
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
            if (!isDraggingWindow && my < 80) { isDraggingWindow = true; dragOffsetX = mx; dragOffsetY = my; }
            if (isDraggingWindow) {
                int wx, wy; glfwGetWindowPos(window, &wx, &wy);
                glfwSetWindowPos(window, wx + (int)mx - (int)dragOffsetX, wy + (int)my - (int)dragOffsetY);
            }
        } else isDraggingWindow = false;

        ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplGlfw_NewFrame(); ImGui::NewFrame();
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize); ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::Begin("Dropzone", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground);
        
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 sz = ImGui::GetIO().DisplaySize;

        // 🎨 UI DESIGN
        dl->AddRectFilled(ImVec2(0, 0), sz, IM_COL32(15, 15, 18, 245), 28.0f); // Darker Glass
        dl->AddRect(ImVec2(0, 0), sz, IM_COL32(255, 255, 255, 40), 28.0f, 0, 1.5f);
        dl->AddRectFilled(ImVec2(0, 0), ImVec2(250, sz.y), IM_COL32(255, 255, 255, 8), 28.0f, ImDrawFlags_RoundCornersLeft);

        // Traffic Lights
        dl->AddCircleFilled(ImVec2(35, 35), 7, IM_COL32(255, 69, 58, 255));
        dl->AddCircleFilled(ImVec2(60, 35), 7, IM_COL32(255, 204, 0, 255));
        dl->AddCircleFilled(ImVec2(85, 35), 7, IM_COL32(52, 199, 89, 255));

        // Sidebar
        ImGui::SetCursorPos(ImVec2(25, 110));
        auto AppleNav = [&](const char* label, int id) {
            bool active = (selectedTab == id); ImVec2 p = ImGui::GetCursorScreenPos();
            if (active) dl->AddRectFilled(ImVec2(p.x - 10, p.y - 5), ImVec2(p.x + 210, p.y + 35), IM_COL32(0, 122, 255, 255), 14.0f);
            if (ImGui::Selectable(label, active, 0, ImVec2(200, 30))) selectedTab = id;
            ImGui::Spacing(); ImGui::Spacing();
        };
        AppleNav("   All History", 0); AppleNav("   Files & Drops", 1); AppleNav("   Images", 2);

        // Content Area
        ImGui::SetCursorPos(ImVec2(280, 100));
        if (ImGui::BeginChild("List", ImVec2(530, 520), false, ImGuiWindowFlags_NoBackground)) {
            for (int i = 0; i < (int)history.size(); i++) {
                if (selectedTab == 1 && history[i].type != "File") continue;
                if (selectedTab == 2 && history[i].type != "Image") continue;

                ImGui::PushID(i); ImVec2 p = ImGui::GetCursorScreenPos();
                bool hov = ImGui::IsMouseHoveringRect(p, ImVec2(p.x + 510, p.y + 90));
                
                dl->AddRectFilled(p, ImVec2(p.x + 510, p.y + 90), hov ? IM_COL32(255, 255, 255, 30) : IM_COL32(255, 255, 255, 15), 20.0f);
                
                // Image Preview Rendering
                if (history[i].type == "Image" && history[i].imagePreview) {
                    dl->AddRectFilled(ImVec2(p.x + 15, p.y + 15), ImVec2(p.x + 75, p.y + 75), IM_COL32(0, 0, 0, 100), 10.0f);
                    dl->AddText(ImVec2(p.x + 90, p.y + 20), IM_COL32(52, 199, 89, 255), "IMAGE CAPTURED");
                    dl->AddText(ImVec2(p.x + 90, p.y + 45), IM_COL32(200, 200, 200, 255), "Live preview active...");
                } else {
                    ImU32 col = (history[i].type == "File") ? IM_COL32(255, 204, 0, 255) : IM_COL32(0, 122, 255, 255);
                    dl->AddText(ImVec2(p.x + 20, p.y + 20), col, history[i].type.c_str());
                    std::string t = std::string(history[i].text.begin(), history[i].text.end());
                    if (t.length() > 60) t = t.substr(0, 57) + "...";
                    dl->AddText(ImVec2(p.x + 20, p.y + 45), IM_COL32(255, 255, 255, 255), t.c_str());
                }

                if (ImGui::InvisibleButton("##C", ImVec2(510, 90))) {
                    if (history[i].type != "Image") {
                        OpenClipboard(nullptr); EmptyClipboard();
                        HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, (history[i].text.size() + 1) * sizeof(wchar_t));
                        memcpy(GlobalLock(h), history[i].text.c_str(), (history[i].text.size() + 1) * sizeof(wchar_t));
                        GlobalUnlock(h); SetClipboardData(CF_UNICODETEXT, h); CloseClipboard();
                        isVisible = false;
                    }
                }
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 100); ImGui::PopID();
            }
            ImGui::EndChild();
        }
        ImGui::End(); ImGui::Render(); glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData()); glfwSwapBuffers(window);
    }
    GdiplusShutdown(gst); return 0;
}

