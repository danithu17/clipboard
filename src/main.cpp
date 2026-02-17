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
double dragOffsetX, dragOffsetY;
int selectedTab = 0; // 0: All, 1: Links, 2: Code

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
    style.FrameRounding = 10.0f;
    style.PopupRounding = 12.0f;
    style.WindowPadding = ImVec2(0, 0);
    style.ItemSpacing = ImVec2(0, 0);

    ImVec4* colors = style.Colors;
    if (dark) {
        colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.12f, 0.98f);
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
                    std::string type = "Text";
                    if (currentText.find(L"http") != std::string::npos) type = "Link";
                    else if (currentText.find(L"{") != std::string::npos || currentText.find(L";") != std::string::npos) type = "Code";
                    history.insert(history.begin(), { currentText, GetCurrentTimeString(), type });
                    if (history.size() > 50) history.pop_back();
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
    GLFWwindow* window = glfwCreateWindow(500, 700, "AppleClip Pro", NULL, NULL);
    glfwMakeContextCurrent(window);
    HWND hwnd = glfwGetWin32Window(window);
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ApplyModernSequoiaTheme(isDarkMode);
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        UpdateClipboard();

        double mx, my; glfwGetCursorPos(window, &mx, &my);
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
            if (!isDragging && my < 50 && mx > 100) { isDragging = true; dragOffsetX = mx; dragOffsetY = my; }
            if (isDragging) {
                int wx, wy; glfwGetWindowPos(window, &wx, &wy);
                glfwSetWindowPos(window, wx + (int)mx - (int)dragOffsetX, wy + (int)my - (int)dragOffsetY);
            }
            // Traffic Light Logic
            if (my > 18 && my < 38) {
                if (mx > 18 && mx < 38) glfwSetWindowShouldClose(window, true);
                if (mx > 38 && mx < 58) ShowWindow(hwnd, SW_MINIMIZE);
            }
        } else isDragging = false;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("Main", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBackground);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImU32 bg = ImGui::GetColorU32(ImGui::GetStyle().Colors[ImGuiCol_WindowBg]);
        dl->AddRectFilled(ImVec2(0, 0), ImGui::GetIO().DisplaySize, bg, 20.0f);

        // Sidebar Background
        dl->AddRectFilled(ImVec2(0, 0), ImVec2(150, 700), ImGui::GetColorU32(ImVec4(0,0,0,0.05f)), 20.0f, ImDrawFlags_RoundCornersLeft);

        // Traffic Lights
        dl->AddCircleFilled(ImVec2(28, 28), 6.0f, IM_COL32(255, 69, 58, 255));
        dl->AddCircleFilled(ImVec2(48, 28), 6.0f, IM_COL32(255, 204, 0, 255));
        dl->AddCircleFilled(ImVec2(68, 28), 6.0f, IM_COL32(52, 199, 89, 255));

        // SIDEBAR CONTENT
        ImGui::SetCursorPos(ImVec2(15, 70));
        ImGui::BeginGroup();
        auto SidebarBtn = [&](const char* label, int id) {
            bool active = (selectedTab == id);
            if (active) dl->AddRectFilled(ImVec2(10, ImGui::GetCursorScreenPos().y - 5), ImVec2(140, ImGui::GetCursorScreenPos().y + 25), ImGui::GetColorU32(ImVec4(0, 0.48f, 1, 0.2f)), 8.0f);
            if (ImGui::Selectable(label, active, 0, ImVec2(120, 20))) selectedTab = id;
            ImGui::Spacing(); ImGui::Spacing();
        };
        SidebarBtn("  All Clips", 0);
        SidebarBtn("  Links", 1);
        SidebarBtn("  Code", 2);
        ImGui::EndGroup();

        // MAIN CONTENT AREA
        ImGui::SetCursorPos(ImVec2(165, 20));
        ImGui::BeginGroup();
        ImGui::TextDisabled("RECENT HISTORY");
        
        ImGui::SetCursorPosX(165);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 20.0f);
        ImGui::SetNextItemWidth(310);
        ImGui::InputTextWithHint("##S", "Search...", searchBuffer, 128);
        ImGui::PopStyleVar();

        ImGui::SetCursorPos(ImVec2(160, 90));
        if (ImGui::BeginChild("Items", ImVec2(330, 590), false, ImGuiWindowFlags_NoBackground)) {
            for (int i = 0; i < (int)history.size(); i++) {
                std::string typeFilter = (selectedTab == 1) ? "Link" : (selectedTab == 2) ? "Code" : "";
                if (!typeFilter.empty() && history[i].type != typeFilter) continue;

                ImGui::PushID(i);
                std::string utf8 = WStringToString(history[i].text);
                ImVec2 cp = ImGui::GetCursorScreenPos();
                
                bool hov = false;
                if (ImGui::InvisibleButton("##c", ImVec2(310, 55))) {
                    OpenClipboard(nullptr); EmptyClipboard();
                    size_t s = (history[i].text.size() + 1) * sizeof(wchar_t);
                    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, s);
                    memcpy(GlobalLock(h), history[i].text.c_str(), s);
                    GlobalUnlock(h); SetClipboardData(CF_UNICODETEXT, h); CloseClipboard();
                }
                hov = ImGui::IsItemHovered();

                ImU32 cBg = hov ? ImGui::GetColorU32(ImVec4(0.5f,0.5f,0.5f,0.12f)) : ImGui::GetColorU32(ImVec4(0.5f,0.5f,0.5f,0.05f));
                dl->AddRectFilled(cp, ImVec2(cp.x + 310, cp.y + 55), cBg, 12.0f);
                
                dl->AddText(ImVec2(cp.x+12, cp.y+10), ImGui::GetColorU32(ImVec4(0, 0.48f, 1, 1)), history[i].type.c_str());
                dl->AddText(ImVec2(cp.x+12, cp.y+30), ImGui::GetColorU32(ImGui::GetStyle().Colors[ImGuiCol_Text]), utf8.substr(0, 35).c_str());
                dl->AddText(ImVec2(cp.x+260, cp.y+10), ImGui::GetColorU32(ImVec4(0.5f,0.5f,0.5f,0.6f)), history[i].time.c_str());

                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8);
                ImGui::PopID();
            }
            ImGui::EndChild();
        }
        ImGui::EndGroup();

        ImGui::End();
        ImGui::Render();
        glClearColor(0,0,0,0);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
        Sleep(10);
    }
    return 0;
}

