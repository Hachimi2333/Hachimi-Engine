#include "ImGui/ImGuiLayer.h"
#include "ImGui/ThemeConfig.h"

#include "Core/Application.h"
#include "Core/Log.h"
#include "Utils/FileSystem.h"
#include "Utils/PlatformUtils.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <algorithm>
#include <filesystem>

namespace HachimiEngine
{
    namespace
    {
        // Base UI font size at 100% display scaling. Fonts are rasterized at BaseFontSize * uiScale and scaled back by
        // FontScaleMain, keeping the same logical size while staying crisp on high-DPI displays.
        constexpr float BaseFontSize = 18.0f;
        constexpr const char* InterFontFileName = "Inter-Regular.ttf";

        std::filesystem::path GetInterFontPath()
        {
            return PlatformUtils::GetExecutableDirectory() / "Assets" / "Fonts" / InterFontFileName;
        }

        void LoadUiFont(ImGuiIO& io, GLFWwindow* window)
        {
            float contentScaleX = 1.0f;
            float contentScaleY = 1.0f;
            glfwGetWindowContentScale(window, &contentScaleX, &contentScaleY);
            const float uiScale = std::max(1.0f, std::max(contentScaleX, contentScaleY));

            // ImGui 1.92+ applies global font scaling through Style.FontScaleMain; io.FontGlobalScale is a legacy knob.
            ImGui::GetStyle().FontScaleMain = 1.0f / uiScale;

            const std::filesystem::path fontPath = GetInterFontPath();
            if (FileSystem::Exists(fontPath))
            {
                ImFont* interFont = io.Fonts->AddFontFromFileTTF(fontPath.string().c_str(), BaseFontSize * uiScale);
                if (interFont != nullptr)
                {
                    io.FontDefault = interFont;
                    HE_CORE_INFO("Loaded ImGui font {} rasterized at {} px (UI scale {:.2f})", fontPath.string(), BaseFontSize * uiScale, uiScale);
                    return;
                }

                HE_CORE_WARN("Failed to parse ImGui font {}", fontPath.string());
            }
            else
            {
                HE_CORE_WARN("ImGui font not found at {}, falling back to the default font", fontPath.string());
            }

            // Keep the fallback at the same logical size and rasterize it at DPI-scaled resolution.
            ImFontConfig fallbackConfig;
            fallbackConfig.SizePixels = BaseFontSize * uiScale;
            io.FontDefault = io.Fonts->AddFontDefault(&fallbackConfig);
        }
    }

    ImGuiLayer::ImGuiLayer()
        : Layer("ImGuiLayer")
    {
    }

    void ImGuiLayer::OnAttach()
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

        // Apply the centralized square, blue-accented dark editor theme.
        ThemeConfig::Apply(ImGui::GetStyle());

        auto* window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
        LoadUiFont(io, window);
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 460");
    }

    void ImGuiLayer::OnDetach()
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void ImGuiLayer::OnEvent(Event& event)
    {
        // Block mouse and keyboard events from reaching layers when ImGui owns them.
        const ImGuiIO& io = ImGui::GetIO();
        if (event.IsInCategory(EventCategoryMouse) && io.WantCaptureMouse)
        {
            event.Handled = true;
        }
        if (event.IsInCategory(EventCategoryKeyboard) && io.WantCaptureKeyboard)
        {
            event.Handled = true;
        }
    }

    void ImGuiLayer::Begin()
    {
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void ImGuiLayer::End()
    {
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if ((ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) != 0)
        {
            // Render secondary viewports (panels dragged outside the main window) and restore the main GLFW context
            // before the application swaps the main window buffers.
            GLFWwindow* mainContext = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(mainContext);
        }
    }
}
