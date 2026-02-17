#include <iostream>
#include <vector>
#include <string>
#include <windows.h>
#include <shlobj.h> // For Startup path
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
bool isVisible = false; // Start hidden for background work
double dragOffsetX, dragOffsetY;
int selectedTab = 0;

// Function to add app to Windows Startup
void RegisterStartup() {
    TCHAR szPath[MAX_PATH];
    GetModuleFileName(NULL, szPath, MAX_PATH);
    HKEY hKey;
    RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey);
    RegSetValueEx(hKey, "AppleClipPro", 0, REG_SZ, (const BYTE*)szPath, strlen(szPath) + 1);
    RegCloseKey(hKey);
}

std::string GetCurrentTimeString() {
    time_t now = time(0); struct tm tstruct; char buf[10];
    localtime_s(&tstruct, &now); strftime(buf, sizeof(buf), "%H:%M", &tstruct);
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

int main() {
    // Add to Startup automatically
    RegisterStartup();

    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE); // Hidden by default

    GLFWwindow* window = glfwCreateWindow(650, 500, "AppleClip Pro", NULL, NULL);
    if (!window) return 1;
    glfwMakeContextCurrent(window);
    HWND hwnd = glfwGetWin32Window(window);

    // Register Alt + Shift + V (0x56 is V)
    if (!RegisterHotKey(hwnd, 1, MOD_ALT | MOD_SHIFT | MOD_NOREPEAT, 0x56)) {
        MessageBox(hwnd, "Hotkey Alt+Shift+V registration failed!", "Error", MB_ICONERROR);
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ApplyModernSequoiaTheme(isDarkMode);
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    while (!glfwWindowShouldClose(window)) {
        MSG msg;
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_HOTKEY) {
                isVisible = !isVisible;
                if (isVisible) {
                    ShowWindow(hwnd, SW_SHOW);
                    ShowWindow(hwnd, SW_RESTORE);
                    SetForegroundWindow(hwnd);
                } else {
                    ShowWindow(hwnd, SW_HIDE);
                }
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        glfwPollEvents();
        
        // Background Clipboard Tracking
        if (OpenClipboard(nullptr)) {
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

        if (!isVisible) { Sleep(20); continue; }

        // Start Rendering
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("Main", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBackground);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImU32 bg = ImGui::GetColorU32(ImGui::GetStyle().Colors[ImGuiCol_WindowBg]);
        dl->AddRectFilled(ImVec2(0, 0), ImGui::GetIO().DisplaySize, bg, 20.0f);

        // Movement Logic
        double mx, my; glfwGetCursorPos(window, &mx, &my);
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
            if (!isDragging && my < 50) { isDragging = true; dragOffsetX = mx; dragOffsetY = my; }
            if (isDragging) {
                int wx, wy; glfwGetWindowPos(window, &wx, &wy);
                glfwSetWindowPos(window, wx + (int)mx - (int)dragOffsetX, wy + (int)my - (int)dragOffsetY);
            }
            // Traffic Light Close/Hide
            if (my > 15 && my < 40) {
                if (mx > 20 && mx < 40) glfwSetWindowShouldClose(window, true);
                if (mx > 40 && mx < 60) { isVisible = false; ShowWindow(hwnd, SW_HIDE); }
            }
        } else isDragging = false;

        // UI Design (Sidebar + List)
        dl->AddRectFilled(ImVec2(0, 0), ImVec2(180, 500), ImGui::GetColorU32(ImVec4(0,0,0,0.05f)), 20.0f, ImDrawFlags_RoundCornersLeft);
        dl->AddCircleFilled(ImVec2(30, 30), 6.5f, IM_COL32(255, 69, 58, 255));
        dl->AddCircleFilled(ImVec2(50, 30), 6.5f, IM_COL32(255, 204, 0, 255));
        dl->AddCircleFilled(ImVec2(70, 30), 6.5f, IM_COL32(52, 199, 89, 255));

        ImGui::SetCursorPos(ImVec2(20, 80));
        auto Nav = [&](const char* label, int id) {
            bool active = (selectedTab == id);
            if (active) dl->AddRectFilled(ImVec2(10, ImGui::GetCursorScreenPos().y - 5), ImVec2(170, ImGui::GetCursorScreenPos().y + 30), ImGui::GetColorU32(ImVec4(0, 0.48f, 1, 0.2f)), 10.0f);
            if (ImGui::Selectable(label, active, 0, ImVec2(140, 25))) selectedTab = id;
            ImGui::Spacing();
        };
        Nav("  All Clips", 0); Nav("  Links", 1); Nav("  Code", 2);

        ImGui::SetCursorPos(ImVec2(200, 30));
        ImGui::SetNextItemWidth(420);
        ImGui::InputTextWithHint("##S", "Search history...", searchBuffer, 128);

        ImGui::SetCursorPos(ImVec2(200, 90));
        if (ImGui::BeginChild("List", ImVec2(430, 380), false, ImGuiWindowFlags_NoBackground)) {
            for (int i = 0; i < (int)history.size(); i++) {
                std::string filter = (selectedTab == 1) ? "Link" : (selectedTab == 2) ? "Code" : "";
                if (!filter.empty() && history[i].type != filter) continue;
                
                ImGui::PushID(i);
                std::string utf8 = WStringToString(history[i].text);
                ImVec2 cp = ImGui::GetCursorScreenPos();
                if (ImGui::InvisibleButton("##b", ImVec2(420, 60))) {
                    OpenClipboard(nullptr); EmptyClipboard();
                    size_t s = (history[i].text.size() + 1) * sizeof(wchar_t);
                    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, s);
                    memcpy(GlobalLock(h), history[i].text.c_str(), s);
                    GlobalUnlock(h); SetClipboardData(CF_UNICODETEXT, h); CloseClipboard();
                }
                bool h = ImGui::IsItemHovered();
                dl->AddRectFilled(cp, ImVec2(cp.x+420, cp.y+60), h ? IM_COL32(100,100,100,40) : IM_COL32(100,100,100,15), 15.0f);
                dl->AddText(ImVec2(cp.x+15, cp.y+12), IM_COL32(0, 122, 255, 255), history[i].type.c_str());
                dl->AddText(ImVec2(cp.x+15, cp.y+32), IM_COL32(255,255,255,255), utf8.substr(0, 50).c_str());
                dl->AddText(ImVec2(cp.x+360, cp.y+12), IM_COL32(150,150,150,200), history[i].time.c_str());
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10);
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
    UnregisterHotKey(hwnd, 1);
    glfwTerminate();
    return 0;
}