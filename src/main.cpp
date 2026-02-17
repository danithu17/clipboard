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
    std::string type; 
};

std::vector<ClipItem> history;
char searchBuffer[128] = "";
std::wstring lastCapturedText = L"";
bool isDarkMode = true;
bool isDragging = false;
double dragOffsetX, dragOffsetY;

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
        colors[ImGuiCol_WindowBg] = ImVec4(0.09f, 0.09f, 0.11f, 0.96f);
        colors[ImGuiCol_Text] = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);
    } else {
        colors[ImGuiCol_WindowBg] = ImVec4(0.96f, 0.96f, 0.98f, 0.96f);
        colors[ImGuiCol_Text] = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.88f, 0.88f, 0.90f, 1.00f);
    }
    colors[ImGuiCol_Button] = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_Header] = ImVec4(0.00f, 0.48f, 1.00f, 0.20f);
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

    GLFWwindow* window = glfwCreateWindow(420, 720, "AppleClip", NULL, NULL);
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

        // Window Dragger
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
            double x, y; glfwGetCursorPos(window, &x, &y);
            if (!isDragging && y < 50) { isDragging = true; dragOffsetX = x; dragOffsetY = y; }
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

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec4 bgCol = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
        dl->AddRectFilled(ImVec2(0, 0), ImGui::GetIO().DisplaySize, ImGui::GetColorU32(bgCol), 18.0f);

        // --- TRAFFIC LIGHTS CONTROLS ---
        ImVec2 tl = ImGui::GetCursorScreenPos();
        
        // Red (Close)
        ImGui::SetCursorPos(ImVec2(18, 18));
        if (ImGui::InvisibleButton("##Close", ImVec2(15, 15))) glfwSetWindowShouldClose(window, true);
        dl->AddCircleFilled(ImVec2(tl.x + 25, tl.y + 25), 6.0f, IM_COL32(255, 69, 58, 255));

        // Yellow (Minimize)
        ImGui::SetCursorPos(ImVec2(38, 18));
        if (ImGui::InvisibleButton("##Min", ImVec2(15, 15))) ShowWindow(hwnd, SW_MINIMIZE);
        dl->AddCircleFilled(ImVec2(tl.x + 45, tl.y + 25), 6.0f, IM_COL32(255, 204, 0, 255));

        // Green (Reset Position - Optional)
        dl->AddCircleFilled(ImVec2(tl.x + 65, tl.y + 25), 6.0f, IM_COL32(52, 199, 89, 255));

        // App Icon & Name
        ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth()/2 - 20, 18));
        ImGui::TextDisabled("( )"); // Placeholder for Icon
        
        ImGui::SetCursorPos(ImVec2(20, 60));
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "CLIPBOARD");
        
        ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth() - 95, 58));
        if (ImGui::SmallButton(isDarkMode ? "Light" : "Dark")) { isDarkMode = !isDarkMode; ApplyAppleTheme(isDarkMode); }
        ImGui::SameLine();
        if (ImGui::SmallButton("Clear")) history.clear();

        ImGui::SetCursorPos(ImVec2(20, 95));
        ImGui::SetNextItemWidth(ImGui::GetWindowWidth() - 40);
        ImGui::InputTextWithHint("##Search", "Search History...", searchBuffer, 128);
        
        ImGui::SetCursorPos(ImVec2(0, 145));
        if (ImGui::BeginChild("ScrollRegion", ImVec2(0, 0), false, ImGuiWindowFlags_NoBackground)) {
            for (int i = 0; i < (int)history.size(); i++) {
                ImGui::PushID(i);
                std::string utf8 = WStringToString(history[i].text);
                
                ImGui::SetCursorPosX(20);
                ImGui::BeginGroup();
                
                // Content Card Hover Logic
                ImVec2 cardMin = ImGui::GetCursorScreenPos();
                bool selected = ImGui::Selectable("##item", false, 0, ImVec2(ImGui::GetWindowWidth() - 40, 50));
                ImVec2 cardMax = ImGui::GetItemRectMax();
                
                if (ImGui::IsItemHovered()) {
                    dl->AddRectFilled(cardMin, cardMax, ImGui::GetColorU32(ImVec4(0, 0.48f, 1, 0.15f)), 10.0f);
                    ImGui::BeginTooltip(); ImGui::TextUnformatted(utf8.c_str()); ImGui::EndTooltip();
                }

                if (selected) {
                    OpenClipboard(nullptr); EmptyClipboard();
                    size_t s = (history[i].text.size() + 1) * sizeof(wchar_t);
                    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, s);
                    memcpy(GlobalLock(h), history[i].text.c_str(), s);
                    GlobalUnlock(h); SetClipboardData(CF_UNICODETEXT, h); CloseClipboard();
                }

                // Draw Text over the selectable
                ImGui::SetCursorScreenPos(ImVec2(cardMin.x + 10, cardMin.y + 8));
                ImGui::TextColored(ImVec4(0, 0.48f, 1, 1), "[%s]", history[i].type.c_str());
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 0.7f), history[i].time.c_str());
                
                ImGui::SetCursorScreenPos(ImVec2(cardMin.x + 10, cardMin.y + 28));
                ImGui::Text(utf8.substr(0, 45).c_str());
                
                ImGui::EndGroup();
                ImGui::Spacing();
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

