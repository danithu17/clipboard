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

struct ClipItem { std::wstring text; bool pinned = false; std::string icon = "T"; };
std::vector<ClipItem> history;
char searchBuffer[128] = "";
std::wstring lastCapturedText = L"";
bool isDarkMode = true;
bool isDragging = false;
double dragOffsetX, dragOffsetY;

std::string WStringToString(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string str(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &str[0], size, NULL, NULL);
    return str;
}

void ApplyApplePremiumTheme(bool dark) {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 18.0f;
    style.FrameRounding = 12.0f;
    style.ItemSpacing = ImVec2(8, 12);
    style.WindowPadding = ImVec2(0, 0); // Content margin api manually handle karamu

    ImVec4* colors = style.Colors;
    if (dark) {
        colors[ImGuiCol_WindowBg] = ImVec4(0.09f, 0.09f, 0.11f, 0.98f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
        colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    } else {
        colors[ImGuiCol_WindowBg] = ImVec4(0.98f, 0.98f, 1.00f, 0.98f);
        colors[ImGuiCol_ChildBg] = ImVec4(0.94f, 0.94f, 0.96f, 1.00f);
        colors[ImGuiCol_Text] = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
    }
    colors[ImGuiCol_Header] = ImVec4(0.00f, 0.48f, 1.00f, 0.30f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.00f, 0.48f, 1.00f, 0.50f);
    colors[ImGuiCol_Button] = ImVec4(0, 0, 0, 0); // Ghost buttons
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
                    std::string icon = (currentText.find(L"http") != std::string::npos) ? "L" : "T";
                    history.insert(history.begin(), { currentText, false, icon });
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
    GLFWwindow* window = glfwCreateWindow(420, 680, "AppleClip", NULL, NULL);
    glfwMakeContextCurrent(window);
    HWND hwnd = glfwGetWin32Window(window);
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ApplyApplePremiumTheme(isDarkMode);
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
        ImGui::Begin("AppleUI", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoBackground);
        
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec4 bg = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
        dl->AddRectFilled(ImVec2(0, 0), ImGui::GetIO().DisplaySize, ImGui::GetColorU32(bg), 18.0f);

        // Sidebar look (Left vertical accent)
        dl->AddRectFilled(ImVec2(0, 0), ImVec2(5, 680), IM_COL32(0, 122, 255, 255), 18.0f, ImDrawFlags_RoundCornersLeft);

        // Header Area
        ImGui::SetCursorPos(ImVec2(20, 18));
        dl->AddCircleFilled(ImVec2(p.x + 25, p.y + 20), 6.0f, IM_COL32(255, 69, 58, 255));
        dl->AddCircleFilled(ImVec2(p.x + 45, p.y + 20), 6.0f, IM_COL32(255, 204, 0, 255));
        dl->AddCircleFilled(ImVec2(p.x + 65, p.y + 20), 6.0f, IM_COL32(52, 199, 89, 255));

        ImGui::SetCursorPos(ImVec2(20, 50));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        ImGui::Text("CLIPBOARD");
        ImGui::PopStyleColor();
        
        ImGui::SetCursorPos(ImVec2(ImGui::GetWindowWidth() - 90, 48));
        if (ImGui::SmallButton("Clear")) history.clear();
        ImGui::SameLine();
        if (ImGui::SmallButton(isDarkMode ? "Light" : "Dark")) { isDarkMode = !isDarkMode; ApplyApplePremiumTheme(isDarkMode); }

        // Search Bar (Apple Style)
        ImGui::SetCursorPos(ImVec2(20, 80));
        ImGui::SetNextItemWidth(ImGui::GetWindowWidth() - 40);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 10));
        ImGui::InputTextWithHint("##Search", "Search History", searchBuffer, 128);
        ImGui::PopStyleVar();

        ImGui::SetCursorPos(ImVec2(0, 130));
        if (ImGui::BeginChild("Items", ImVec2(0, 0), false, ImGuiWindowFlags_NoBackground)) {
            for (int i = 0; i < (int)history.size(); i++) {
                ImGui::PushID(i);
                std::string utf8 = WStringToString(history[i].text);
                
                ImGui::SetCursorPosX(15);
                ImGui::BeginGroup();
                
                // Icon tag
                ImGui::TextColored(ImVec4(0, 0.48f, 1, 1), history[i].icon.c_str());
                ImGui::SameLine(40);
                
                if (ImGui::Selectable(utf8.substr(0, 40).c_str(), false, 0, ImVec2(ImGui::GetWindowWidth()-55, 40))) {
                    OpenClipboard(nullptr); EmptyClipboard();
                    size_t s = (history[i].text.size() + 1) * sizeof(wchar_t);
                    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, s);
                    memcpy(GlobalLock(h), history[i].text.c_str(), s);
                    GlobalUnlock(h); SetClipboardData(CF_UNICODETEXT, h); CloseClipboard();
                }
                ImGui::EndGroup();
                
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip(); ImGui::TextUnformatted(utf8.c_str()); ImGui::EndTooltip();
                    dl->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImVec4(0, 0.48f, 1, 0.1f), 8.0f);
                }
                
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5);
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
    }
    return 0;
}

