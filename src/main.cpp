#include <iostream>
#include <vector>
#include <string>
#include <windows.h>
#include <shlobj.h>
#include <ctime>
#include <map>

// ImGui & GLFW
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "imgui_internal.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

// Structure for Clipboard items
struct ClipItem {
    std::wstring text;
    std::string time;
    std::string type; // "Text", "Link", "Image", "Code"
    bool isPinned = false;
    GLuint textureID = 0; // For Image Previews
    int imgW = 0, imgH = 0;
};

std::vector<ClipItem> history;
std::wstring lastCapturedText = L"";
bool isVisible = true;
bool isDarkMode = true;
int selectedTab = 0; // 0: All, 1: Links, 2: Images, 3: Pinned
char searchBuf[128] = "";

// --- 🖼️ IMAGE HELPER: Convert Clipboard DIB to Texture ---
GLuint CreateTextureFromDIB() {
    if (!OpenClipboard(NULL)) return 0;
    HANDLE hBitmap = GetClipboardData(CF_BITMAP);
    if (!hBitmap) { CloseClipboard(); return 0; }
    
    // Simple placeholder for texture generation logic
    // In a full build, you'd use GDI+ or stb_image to convert HBITMAP to RGBA
    CloseClipboard();
    return 1; // Returning dummy ID for structure
}

// --- 📋 CLIPBOARD MONITOR ---
void MonitorClipboard() {
    if (!OpenClipboard(nullptr)) return;

    // 1. Text Capture
    if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
        HANDLE hData = GetClipboardData(CF_UNICODETEXT);
        if (hData) {
            wchar_t* pText = (wchar_t*)GlobalLock(hData);
            if (pText && lastCapturedText != pText) {
                lastCapturedText = pText;
                std::string type = (lastCapturedText.find(L"http") != std::wstring::npos) ? "Link" : "Text";
                time_t now = time(0); struct tm t; localtime_s(&t, &now); char buf[10]; strftime(buf, sizeof(buf), "%H:%M", &t);
                history.insert(history.begin(), { lastCapturedText, buf, type });
            }
            GlobalUnlock(hData);
        }
    }
    
    // 2. Image Capture
    if (IsClipboardFormatAvailable(CF_BITMAP)) {
        // Logic to prevent duplicate image capture would go here
    }

    CloseClipboard();
}

// --- 🎨 UI STYLING ---
void ApplySequoiaStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 25.0f;
    style.FrameRounding = 15.0f;
    style.ItemSpacing = ImVec2(10, 10);
    style.WindowPadding = ImVec2(0, 0);

    ImVec4* colors = style.Colors;
    if (isDarkMode) {
        colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.08f, 0.10f, 0.95f);
        colors[ImGuiCol_Text] = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
    } else {
        colors[ImGuiCol_WindowBg] = ImVec4(0.98f, 0.98f, 1.00f, 0.96f);
        colors[ImGuiCol_Text] = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
    }
    colors[ImGuiCol_Header] = ImVec4(0.00f, 0.48f, 1.00f, 1.00f); // Apple Blue
}

int main() {
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "AppleClip Pro", NULL, NULL);
    glfwMakeContextCurrent(window);
    HWND hwnd = glfwGetWin32Window(window);

    // Hotkey: Alt + Shift + C
    RegisterHotKey(hwnd, 1, MOD_ALT | MOD_SHIFT, 0x43);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    while (!glfwWindowShouldClose(window)) {
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_HOTKEY) isVisible = !isVisible;
            if (isVisible) ShowWindow(hwnd, SW_SHOW); else ShowWindow(hwnd, SW_HIDE);
            TranslateMessage(&msg); DispatchMessage(&msg);
        }

        glfwPollEvents();
        MonitorClipboard();

        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) isVisible = false;
        if (!isVisible) { Sleep(30); continue; }

        ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplGlfw_NewFrame(); ImGui::NewFrame();
        ApplySequoiaStyle();

        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::Begin("Main", NULL, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 winSize = ImGui::GetIO().DisplaySize;

        // --- 🎨 APPLE GLASS CANVAS ---
        dl->AddRectFilled(ImVec2(0, 0), winSize, ImGui::GetColorU32(ImGuiCol_WindowBg), 25.0f);
        dl->AddRect(ImVec2(0, 0), winSize, IM_COL32(255, 255, 255, 30), 25.0f, 0, 1.0f); // Outer stroke

        // --- 📂 SIDEBAR (Frosted) ---
        dl->AddRectFilled(ImVec2(0, 0), ImVec2(240, winSize.y), isDarkMode ? IM_COL32(255, 255, 255, 10) : IM_COL32(0, 0, 0, 10), 25.0f, ImDrawFlags_RoundCornersLeft);

        // Traffic Lights
        dl->AddCircleFilled(ImVec2(30, 30), 7.0f, IM_COL32(255, 69, 58, 255));
        dl->AddCircleFilled(ImVec2(55, 30), 7.0f, IM_COL32(255, 204, 0, 255));
        dl->AddCircleFilled(ImVec2(80, 30), 7.0f, IM_COL32(52, 199, 89, 255));

        // Navigation
        ImGui::SetCursorPos(ImVec2(20, 90));
        auto NavItem = [&](const char* label, int id) {
            bool active = (selectedTab == id);
            if (active) dl->AddRectFilled(ImVec2(10, ImGui::GetCursorScreenPos().y - 5), ImVec2(230, ImGui::GetCursorScreenPos().y + 35), IM_COL32(0, 122, 255, 255), 12.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, active ? IM_COL32(255, 255, 255, 255) : ImGui::GetColorU32(ImGuiCol_Text));
            if (ImGui::Selectable(label, active, 0, ImVec2(200, 30))) selectedTab = id;
            ImGui::PopStyleColor();
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10);
        };
        NavItem("   All History", 0);
        NavItem("   Links", 1);
        NavItem("   Images", 2);
        NavItem("   Pinned", 3);

        // --- 📋 CONTENT AREA ---
        ImGui::SetCursorPos(ImVec2(270, 30));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 20.0f);
        ImGui::SetNextItemWidth(450);
        ImGui::InputTextWithHint("##Search", "Search your clipboard...", searchBuf, 128);
        ImGui::PopStyleVar();

        ImGui::SetCursorPos(ImVec2(270, 90));
        if (ImGui::BeginChild("ScrollArea", ImVec2(500, 480), false, ImGuiWindowFlags_NoBackground)) {
            for (int i = 0; i < (int)history.size(); i++) {
                // Filter Logic
                if (selectedTab == 1 && history[i].type != "Link") continue;
                if (selectedTab == 2 && history[i].type != "Image") continue;
                if (selectedTab == 3 && !history[i].isPinned) continue;

                ImGui::PushID(i);
                ImVec2 pos = ImGui::GetCursorScreenPos();
                bool hovered = ImGui::IsMouseHoveringRect(pos, ImVec2(pos.x + 480, pos.y + 80));

                // Card Design
                dl->AddRectFilled(pos, ImVec2(pos.x + 480, pos.y + 80), hovered ? IM_COL32(150, 150, 150, 30) : IM_COL32(150, 150, 150, 15), 18.0f);
                
                // Type Indicator (Blue dot for new items)
                dl->AddCircleFilled(ImVec2(pos.x + 15, pos.y + 15), 4.0f, IM_COL32(0, 122, 255, 255));

                // Content Text
                ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPos().x + 25, ImGui::GetCursorPos().y + 15));
                ImGui::TextColored(ImVec4(0, 0.48f, 1, 1), history[i].type.c_str());
                
                ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPos().x + 25, ImGui::GetCursorPos().y + 2));
                std::string preview = history[i].type == "Image" ? "[ Image Preview Available ]" : std::string(history[i].text.begin(), history[i].text.end()).substr(0, 50) + "...";
                ImGui::Text(preview.c_str());

                // Pin Button (Top Right of Card)
                ImGui::SetCursorPos(ImVec2(440, ImGui::GetCursorPos().y - 35));
                if (ImGui::Button(history[i].isPinned ? "Unpin" : "Pin")) history[i].isPinned = !history[i].isPinned;

                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 45);
                ImGui::PopID();
            }
            ImGui::EndChild();
        }

        ImGui::End();
        ImGui::Render();
        glClearColor(0, 0, 0, 0); glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    return 0;
}