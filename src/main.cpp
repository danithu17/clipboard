#include <iostream>
#include <vector>
#include <string>
#include <windows.h>
#include <algorithm>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

struct ClipItem { std::wstring text; bool pinned = false; };
std::vector<ClipItem> history;
char searchBuffer[128] = "";
std::wstring lastCapturedText = L"";
bool isDarkMode = true;

// Window dragging logic variables
bool isDragging = false;
double dragOffsetX, dragOffsetY;

std::string WStringToString(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string str(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &str[0], size, NULL, NULL);
    return str;
}

void ApplyMacTheme(bool dark) {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 16.0f;
    style.FrameRounding = 10.0f;
    style.ScrollbarRounding = 12.0f;
    style.WindowBorderSize = 0.0f;

    ImVec4* colors = style.Colors;
    if (dark) {
        colors[ImGuiCol_WindowBg] = ImVec4(0.12f, 0.12f, 0.14f, 0.98f);
        colors[ImGuiCol_Text] = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.18f, 0.18f, 0.20f, 1.00f);
    } else {
        colors[ImGuiCol_WindowBg] = ImVec4(0.95f, 0.95f, 0.97f, 0.98f);
        colors[ImGuiCol_Text] = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
        colors[ImGuiCol_FrameBg] = ImVec4(0.88f, 0.88f, 0.90f, 1.00f);
    }
    colors[ImGuiCol_Button] = colors[ImGuiCol_FrameBg];
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.00f, 0.48f, 1.00f, 1.00f); // Apple Blue
}

void UpdateClipboard() {
    if (!OpenClipboard(nullptr)) return;
    if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
        HANDLE hData = GetClipboardData(CF_UNICODETEXT);
        if (hData) {
            wchar_t* pText = static_cast<wchar_t*>(GlobalLock(hData));
            if (pText) {
                std::wstring currentText(pText);
                GlobalUnlock(hData);
                if (currentText != lastCapturedText && !currentText.empty()) {
                    lastCapturedText = currentText;
                    history.insert(history.begin(), { currentText, false });
                    if (history.size() > 50) history.pop_back();
                }
            }
        }
    }
    CloseClipboard();
}

int main() {
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_DECORATED, GLFW_FALSE); // Remove Windows Titlebar
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_TRUE);

    GLFWwindow* window = glfwCreateWindow(400, 600, "MacClipboard", NULL, NULL);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    HWND hwnd = glfwGetWin32Window(window);
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ApplyMacTheme(isDarkMode);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        UpdateClipboard();

        // Handle Window Dragging
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
            double x, y;
            glfwGetCursorPos(window, &x, &y);
            if (!isDragging && y < 40) { // Drag only from top area
                isDragging = true;
                dragOffsetX = x; dragOffsetY = y;
            }
            if (isDragging) {
                int wx, wy;
                glfwGetWindowPos(window, &wx, &wy);
                glfwSetWindowPos(window, wx + (int)x - (int)dragOffsetX, wy + (int)y - (int)dragOffsetY);
            }
        } else { isDragging = false; }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
        ImGui::Begin("Main", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);

        // Traffic Lights
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 p = ImGui::GetCursorScreenPos();
        dl->AddCircleFilled(ImVec2(p.x + 20, p.y + 20), 6.0f, IM_COL32(255, 95, 86, 255));
        dl->AddCircleFilled(ImVec2(p.x + 40, p.y + 20), 6.0f, IM_COL32(255, 189, 46, 255));
        dl->AddCircleFilled(ImVec2(p.x + 60, p.y + 20), 6.0f, IM_COL32(39, 201, 63, 255));

        // Top Controls
        ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth() - 80, 12));
        if (ImGui::SmallButton(isDarkMode ? "Light" : "Dark")) {
            isDarkMode = !isDarkMode;
            ApplyMacTheme(isDarkMode);
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("X")) glfwSetWindowShouldClose(window, true);

        ImGui::SetCursorPosY(45);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 20.0f);
        ImGui::SetNextItemWidth(-50);
        ImGui::InputTextWithHint("##Search", " Search...", searchBuffer, 128);
        ImGui::PopStyleVar();
        
        ImGui::SameLine();
        if (ImGui::Button("Clear", ImVec2(40, 25))) history.clear();

        ImGui::Separator();

        if (ImGui::BeginChild("List")) {
            for (int i = 0; i < (int)history.size(); i++) {
                std::string utf8 = WStringToString(history[i].text);
                if (ImGui::Selectable(utf8.substr(0, 45).c_str(), false, 0, ImVec2(0, 45))) {
                    if (OpenClipboard(nullptr)) {
                        EmptyClipboard();
                        size_t s = (history[i].text.size() + 1) * sizeof(wchar_t);
                        HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, s);
                        memcpy(GlobalLock(h), history[i].text.c_str(), s);
                        GlobalUnlock(h); SetClipboardData(CF_UNICODETEXT, h); CloseClipboard();
                        lastCapturedText = history[i].text;
                    }
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip(); ImGui::TextUnformatted(utf8.c_str()); ImGui::EndTooltip();
                }
                ImGui::Separator();
            }
            ImGui::EndChild();
        }
        ImGui::End();

        ImGui::Render();
        glClearColor(0,0,0,0);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }
    return 0;
}

