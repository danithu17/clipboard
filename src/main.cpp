#include <windows.h>
#include <dwmapi.h>
#include <gdiplus.h>
#include <shlobj.h>
#include <iostream>
#include <vector>
#include <string>
#include <ctime>
#include <thread>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "opengl32.lib")

using namespace Gdiplus;

// --- Data Structure ---
struct ClipItem {
    std::wstring text; 
    std::string type;  
    std::string time;
    bool isPinned = false;
    GLuint textureID = 0; // For Real Image Preview
    int imgW = 0, imgH = 0;
};

std::vector<ClipItem> history;
std::wstring lastCapturedText = L"";
bool isVisible = true;
bool isDraggingWindow = false;
double dragOffsetX, dragOffsetY;
int selectedTab = 0;

// --- 🖼️ IMAGE HELPER: Convert HBITMAP to OpenGL Texture ---
GLuint LoadTextureFromHBitmap(HBITMAP hBitmap, int* out_width, int* out_height) {
    Bitmap bmp(hBitmap, NULL);
    *out_width = bmp.GetWidth();
    *out_height = bmp.GetHeight();
    
    BitmapData data;
    Rect rect(0, 0, *out_width, *out_height);
    bmp.LockBits(&rect, ImageLockModeRead, PixelFormat32bppARGB, &data);

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, *out_width, *out_height, 0, GL_BGRA, GL_UNSIGNED_BYTE, data.Scan0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    bmp.UnlockBits(&data);
    return texture;
}

// --- 📋 CLIPBOARD MONITOR (Optimized to Prevent Freeze) ---
void MonitorClipboard() {
    static DWORD lastTime = 0;
    if (GetTickCount() - lastTime < 500) return; // Check every 500ms
    lastTime = GetTickCount();

    if (!OpenClipboard(NULL)) return;

    time_t now = time(0); struct tm t; localtime_s(&t, &now); 
    char timeBuf[10]; strftime(timeBuf, sizeof(timeBuf), "%H:%M", &t);

    if (IsClipboardFormatAvailable(CF_HDROP)) {
        HDROP hDrop = (HDROP)GetClipboardData(CF_HDROP);
        if (hDrop) {
            wchar_t filePath[MAX_PATH]; DragQueryFileW(hDrop, 0, filePath, MAX_PATH);
            if (lastCapturedText != filePath) {
                lastCapturedText = filePath;
                history.insert(history.begin(), { filePath, "File", timeBuf, false });
            }
        }
    } else if (IsClipboardFormatAvailable(CF_BITMAP)) {
        HBITMAP hBmp = (HBITMAP)GetClipboardData(CF_BITMAP);
        if (hBmp && lastCapturedText != L"__IMG__") {
            lastCapturedText = L"__IMG__";
            int w, h;
            GLuint tex = LoadTextureFromHBitmap(hBmp, &w, &h);
            history.insert(history.begin(), { L"Clipboard Image", "Image", timeBuf, false, tex, w, h });
        }
    } else if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
        HANDLE hData = GetClipboardData(CF_UNICODETEXT);
        if (hData) {
            wchar_t* pText = (wchar_t*)GlobalLock(hData);
            if (pText && lastCapturedText != pText) {
                lastCapturedText = pText;
                std::string type = (lastCapturedText.find(L"http") != std::wstring::npos) ? "Link" : "Text";
                history.insert(history.begin(), { pText, type, timeBuf, false });
            }
            GlobalUnlock(hData);
        }
    }
    CloseClipboard();
}

// --- 📥 DRAG & DROP CALLBACK ---
void OnFileDrop(GLFWwindow* window, int count, const char** paths) {
    time_t now = time(0); struct tm t; localtime_s(&t, &now); 
    char timeBuf[10]; strftime(timeBuf, sizeof(timeBuf), "%H:%M", &t);
    for (int i = 0; i < count; i++) {
        std::string p = paths[i];
        std::wstring wp(p.begin(), p.end());
        history.insert(history.begin(), { wp, "File", timeBuf, false });
    }
}

int main() {
    GdiplusStartupInput gsi; ULONG_PTR gst; GdiplusStartup(&gst, &gsi, NULL);
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(850, 650, "Apple Sequoia Dropzone", NULL, NULL);
    HWND hwnd = glfwGetWin32Window(window);
    
    // Sequoia Blur
    BOOL darkMode = TRUE; DwmSetWindowAttribute(hwnd, 20, &darkMode, sizeof(darkMode));
    int backdrop = 3; DwmSetWindowAttribute(hwnd, 38, &backdrop, sizeof(backdrop));

    glfwMakeContextCurrent(window);
    glfwSetDropCallback(window, OnFileDrop);
    RegisterHotKey(hwnd, 1, MOD_ALT | MOD_SHIFT, 0x43);

    IMGUI_CHECKVERSION(); ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    ImGui::GetStyle().WindowRounding = 28.0f;

    while (!glfwWindowShouldClose(window)) {
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_HOTKEY) isVisible = !isVisible;
            TranslateMessage(&msg); DispatchMessage(&msg);
        }
        if (isVisible) ShowWindow(hwnd, SW_SHOW); else { ShowWindow(hwnd, SW_HIDE); Sleep(50); continue; }

        glfwPollEvents(); MonitorClipboard();
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) isVisible = false;

        // Window Dragging Logic (Fixed)
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

        // UI Design
        dl->AddRectFilled(ImVec2(0, 0), sz, IM_COL32(10, 10, 15, 245), 28.0f);
        dl->AddRectFilled(ImVec2(0, 0), ImVec2(250, sz.y), IM_COL32(255, 255, 255, 10), 28.0f, ImDrawFlags_RoundCornersLeft);

        // Sidebar Navigation
        ImGui::SetCursorPos(ImVec2(25, 110));
        auto Nav = [&](const char* label, int id) {
            if (selectedTab == id) dl->AddRectFilled(ImVec2(ImGui::GetCursorScreenPos().x - 10, ImGui::GetCursorScreenPos().y - 5), ImVec2(ImGui::GetCursorScreenPos().x + 210, ImGui::GetCursorScreenPos().y + 35), IM_COL32(0, 122, 255, 255), 14.0f);
            if (ImGui::Selectable(label, selectedTab == id, 0, ImVec2(200, 30))) selectedTab = id;
            ImGui::Spacing(); ImGui::Spacing();
        };
        Nav("   All Snippets", 0); Nav("   Files & Drops", 1); Nav("   Images", 2);

        // Content List
        ImGui::SetCursorPos(ImVec2(280, 80));
        if (ImGui::BeginChild("List", ImVec2(530, 540), false, ImGuiWindowFlags_NoBackground)) {
            for (int i = 0; i < (int)history.size(); i++) {
                if (selectedTab == 1 && history[i].type != "File") continue;
                if (selectedTab == 2 && history[i].type != "Image") continue;

                ImGui::PushID(i); ImVec2 p = ImGui::GetCursorScreenPos();
                bool hov = ImGui::IsMouseHoveringRect(p, ImVec2(p.x + 510, p.y + 110));
                dl->AddRectFilled(p, ImVec2(p.x + 510, p.y + 110), hov ? IM_COL32(255, 255, 255, 30) : IM_COL32(255, 255, 255, 15), 20.0f);
                
                // Image Rendering
                if (history[i].type == "Image" && history[i].textureID != 0) {
                    ImGui::SetCursorScreenPos(ImVec2(p.x + 15, p.y + 15));
                    ImGui::Image((void*)(intptr_t)history[i].textureID, ImVec2(80, 80));
                    ImGui::SetCursorScreenPos(ImVec2(p.x + 110, p.y + 25));
                    ImGui::TextColored(ImVec4(0, 0.8f, 0, 1), "IMAGE PREVIEW");
                } else {
                    ImGui::SetCursorScreenPos(ImVec2(p.x + 20, p.y + 20));
                    ImGui::TextColored(ImVec4(0, 0.5f, 1, 1), history[i].type.c_str());
                    ImGui::SetCursorScreenPos(ImVec2(p.x + 20, p.y + 45));
                    std::string t = std::string(history[i].text.begin(), history[i].text.end()).substr(0, 50);
                    ImGui::Text(t.c_str());
                }

                ImGui::SetCursorScreenPos(p);
                if (ImGui::InvisibleButton("##C", ImVec2(510, 110))) {
                    if (history[i].type != "Image") {
                        OpenClipboard(NULL); EmptyClipboard();
                        HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, (history[i].text.size() + 1) * sizeof(wchar_t));
                        memcpy(GlobalLock(h), history[i].text.c_str(), (history[i].text.size() + 1) * sizeof(wchar_t));
                        GlobalUnlock(h); SetClipboardData(CF_UNICODETEXT, h); CloseClipboard();
                        isVisible = false;
                    }
                }
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 120); ImGui::PopID();
            }
            ImGui::EndChild();
        }
        ImGui::End(); ImGui::Render(); glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData()); glfwSwapBuffers(window);
    }
    GdiplusShutdown(gst); return 0;
}

