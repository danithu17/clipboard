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

struct ClipItem { 
    std::wstring text; 
    std::string time; 
    std::string type; // "TEXT", "LINK", "CODE"
};

std::vector<ClipItem> history;
char searchBuffer[128] = "";
std::wstring lastCapturedText = L"";
bool isDarkMode = true;
bool isDragging = false;
double dragOffsetX, dragOffsetY;

// Current time generator
std::string GetCurrentTimeString() {
    time_t now = time(0);
    struct tm tstruct;
    char buf[10];
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

void ApplyAppleTheme(bool dark) {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 18.0f;
    style.FrameRounding = 10.0f;
    style.ScrollbarRounding = 12.0f;
    style.WindowBorderSize = 0.0f;
    style.ItemSpacing = ImVec2(10, 10);

    ImVec4* colors = style.Colors;
    if (dark) {
        colors[ImGuiCol_WindowBg] = ImVec4(0.09f, 0.09f, 0.11f, 0.94f);
        colors[ImGuiCol_Text] = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
    } else {
        colors[ImGuiCol_WindowBg] = ImVec4(0.96f, 0.96f, 0.98f, 0.94f);
        colors[ImGuiCol_Text] = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.88f, 0.88f, 0.90f, 1.00f);
    }
    colors[ImGuiCol_Button] = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_Header] = ImVec4(0.00f, 0.48f, 1.00f, 0.20f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.00f, 0.48f, 1.00f, 0.40f);
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
                    std::string type = "TEXT";
                    if (currentText.find(L"http") != std::string::npos) type = "LINK";
                    else if (currentText.find(L"{") != std::string::npos || currentText.find(L";") != std::string::npos) type = "CODE";
                    
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

    GLFWwindow* window = glfwCreateWindow(420, 700, "AppleClip", NULL, NULL);
    glfwMakeContextCurrent(window);
    HWND hwnd = glfwGetWin32Window(window);
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ApplyAppleTheme(isDarkMode);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        UpdateClipboard();

        // Draggable Logic
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
            double x, y; glfwGetCursorPos(window, &x, &y);
            if (!isDragging && y < 60) { isDragging = true; dragOffsetX = x; dragOffsetY = y; }
            if (isDragging) {
                int wx, wy; glfwGetWindowPos(window, &wx, &wy);
                glfwSetWindowPos(window, wx + (int)x - (int)dragOffsetX, wy + (int)y - (int)dragOffsetY);
            }
        } else isDragging = false;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("MacUI", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBackground);

        // Render Background with shadow effect
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec4 bgCol = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
        dl->AddRectFilled(ImVec2(0, 0), ImGui::GetIO().DisplaySize, ImGui::GetColorU32(bgCol), 18.0f);

        // Apple Traffic Lights
        dl->AddCircleFilled(ImVec2(25, 25), 6.0f, IM_COL32(255, 69, 58, 255));
        dl->AddCircleFilled(ImVec2(45, 25), 6.0f, IM_COL32(255, 204, 0, 255));
        dl->AddCircleFilled(ImVec2(65, 25), 6.0f, IM_COL32(52, 199, 89, 255));

        // Header Title
        ImGui::SetCursorPos(ImVec2(20, 55));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        ImGui::Text("CLIPBOARD HISTORY");
        ImGui::PopStyleColor();

        // Mode and Clear
        ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth() - 95, 52));
        if (ImGui::SmallButton(isDarkMode ? "Light" : "Dark")) { isDarkMode = !isDarkMode; ApplyAppleTheme(isDarkMode); }
        ImGui::SameLine();
        if (ImGui::SmallButton("Clear")) history.clear();

        // Search Bar (Apple Style)
        ImGui::SetCursorPos(ImVec2(20, 85));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 8));
        ImGui::SetNextItemWidth(ImGui::GetWindowWidth() - 40);
        ImGui::InputTextWithHint("##Search", "Search clips...", searchBuffer, 128);
        ImGui::PopStyleVar(2);

        ImGui::SetCursorPos(ImVec2(0, 140));
        if (ImGui::BeginChild("ScrollRegion", ImVec2(0, 0), false, ImGuiWindowFlags_NoBackground)) {
            for (int i = 0; i < (int)history.size(); i++) {
                ImGui::PushID(i);
                std::string utf8 = WStringToString(history[i].text);
                
                // Content Card
                ImGui::SetCursorPosX(15);
                ImGui::BeginGroup();
                
                // Metadata (Type & Time)
                ImGui::TextColored(ImVec4(0, 0.48f, 1, 1), "[%s]", history[i].type.c_str());
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 0.8f), history[i].time.c_str());
                
                if (ImGui::Selectable(utf8.substr(0, 42).c_str(), false, 0, ImVec2(ImGui::GetWindowWidth() - 30, 45))) {
                    OpenClipboard(nullptr); EmptyClipboard();
                    size_t s = (history[i].text.size() + 1) * sizeof(wchar_t);
                    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, s);
                    memcpy(GlobalLock(h), history[i].text.c_str(), s);
                    GlobalUnlock(h); SetClipboardData(CF_UNICODETEXT, h); CloseClipboard();
                }
                ImGui::EndGroup();

                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::TextUnformatted(utf8.c_str());
                    ImGui::EndTooltip();
                    dl->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImVec4(0, 0.48f, 1, 0.1f), 10.0f);
                }
                
                ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();
                ImGui::PopID();
            }
            ImGui::EndChild();
        }
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

