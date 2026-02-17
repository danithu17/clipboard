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

void ApplyModernAppleTheme(bool dark) {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 20.0f;
    style.FrameRounding = 12.0f;
    style.PopupRounding = 12.0f;
    style.ScrollbarRounding = 12.0f;
    style.WindowPadding = ImVec2(20, 20);
    style.ItemSpacing = ImVec2(10, 15);
    style.GrabRounding = 12.0f;

    ImVec4* colors = style.Colors;
    if (dark) {
        colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.08f, 0.09f, 0.94f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_Text] = ImVec4(0.92f, 0.92f, 0.95f, 1.00f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.14f, 0.14f, 0.16f, 1.00f);
    } else {
        colors[ImGuiCol_WindowBg] = ImVec4(0.98f, 0.98f, 1.00f, 0.94f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_Text] = ImVec4(0.12f, 0.12f, 0.15f, 1.00f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.90f, 0.90f, 0.93f, 1.00f);
    }
    colors[ImGuiCol_Header] = ImVec4(0.00f, 0.48f, 1.00f, 0.15f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.00f, 0.48f, 1.00f, 0.25f);
    colors[ImGuiCol_Button] = ImVec4(0, 0, 0, 0);
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

    GLFWwindow* window = glfwCreateWindow(420, 750, "AppleClip Pro", NULL, NULL);
    glfwMakeContextCurrent(window);
    HWND hwnd = glfwGetWin32Window(window);
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ApplyModernAppleTheme(isDarkMode);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        UpdateClipboard();

        double mx, my;
        glfwGetCursorPos(window, &mx, &my);

        // Window Dragger
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
            if (!isDragging && my < 60 && mx > 80) { isDragging = true; dragOffsetX = mx; dragOffsetY = my; }
            if (isDragging) {
                int wx, wy; glfwGetWindowPos(window, &wx, &wy);
                glfwSetWindowPos(window, wx + (int)mx - (int)dragOffsetX, wy + (int)my - (int)dragOffsetY);
            }
        } else isDragging = false;

        // --- TRAFFIC LIGHTS CLICK LOGIC ---
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS && !isDragging) {
            if (my > 15 && my < 35) {
                if (mx > 15 && mx < 35) glfwSetWindowShouldClose(window, true); // Red
                if (mx > 35 && mx < 55) ShowWindow(hwnd, SW_MINIMIZE);          // Yellow
            }
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("AppleSequoia", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBackground);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec4 bgCol = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
        dl->AddRectFilled(ImVec2(0, 0), ImGui::GetIO().DisplaySize, ImGui::GetColorU32(bgCol), 20.0f);

        // Modern Traffic Lights (Clean circles)
        dl->AddCircleFilled(ImVec2(25, 25), 6.5f, IM_COL32(255, 69, 58, 255));
        dl->AddCircleFilled(ImVec2(45, 25), 6.5f, IM_COL32(255, 204, 0, 255));
        dl->AddCircleFilled(ImVec2(65, 25), 6.5f, IM_COL32(52, 199, 89, 255));

        // Center App Label
        ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth()/2 - 40, 18));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 0.7f));
        ImGui::Text("Clipboard");
        ImGui::PopStyleColor();

        // Theme Toggle & Clear
        ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth() - 100, 55));
        if (ImGui::SmallButton(isDarkMode ? "Light" : "Dark")) { isDarkMode = !isDarkMode; ApplyModernAppleTheme(isDarkMode); }
        ImGui::SameLine();
        if (ImGui::SmallButton("Clear")) history.clear();

        // Modern Search Bar (Pill shaped)
        ImGui::SetCursorPos(ImVec2(20, 90));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 25.0f);
        ImGui::SetNextItemWidth(ImGui::GetWindowWidth() - 40);
        ImGui::InputTextWithHint("##Search", "Search clips...", searchBuffer, 128);
        ImGui::PopStyleVar();

        ImGui::SetCursorPos(ImVec2(0, 140));
        if (ImGui::BeginChild("Items", ImVec2(0, 0), false, ImGuiWindowFlags_NoBackground)) {
            for (int i = 0; i < (int)history.size(); i++) {
                ImGui::PushID(i);
                std::string utf8 = WStringToString(history[i].text);
                
                ImGui::SetCursorPosX(15);
                ImVec2 cardPos = ImGui::GetCursorScreenPos();
                
                // Item Container (The Apple Card Look)
                bool hovered = false;
                if (ImGui::InvisibleButton("##card", ImVec2(ImGui::GetWindowWidth() - 30, 60))) {
                    OpenClipboard(nullptr); EmptyClipboard();
                    size_t s = (history[i].text.size() + 1) * sizeof(wchar_t);
                    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, s);
                    memcpy(GlobalLock(h), history[i].text.c_str(), s);
                    GlobalUnlock(h); SetClipboardData(CF_UNICODETEXT, h); CloseClipboard();
                }
                hovered = ImGui::IsItemHovered();

                // Draw Card Background
                ImU32 cardBg = hovered ? ImGui::GetColorU32(ImVec4(0, 0.48f, 1, 0.1f)) : ImGui::GetColorU32(ImVec4(0.5f, 0.5f, 0.5f, 0.03f));
                dl->AddRectFilled(cardPos, ImVec2(cardPos.x + ImGui::GetWindowWidth() - 30, cardPos.y + 60), cardBg, 12.0f);

                // Draw Text & Tags
                dl->AddText(ImVec2(cardPos.x + 15, cardPos.y + 12), ImGui::GetColorU32(ImVec4(0, 0.48f, 1, 1)), history[i].type.c_str());
                dl->AddText(ImVec2(cardPos.x + 60, cardPos.y + 12), ImGui::GetColorU32(ImVec4(0.5f, 0.5f, 0.5f, 0.8f)), history[i].time.c_str());
                dl->AddText(ImVec2(cardPos.x + 15, cardPos.y + 35), ImGui::GetColorU32(ImGui::GetStyle().Colors[ImGuiCol_Text]), utf8.substr(0, 40).c_str());

                if (hovered) { ImGui::BeginTooltip(); ImGui::TextUnformatted(utf8.c_str()); ImGui::EndTooltip(); }

                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10);
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

