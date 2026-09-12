#include "ImGui/ImGuiLayer.h"
#include "ImGui/ThemeConfig.h"

#include "Core/Application.h"
#include "Core/Log.h"
#include "Utils/VirtualFileSystem.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <vector>

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
            // The packaged game keeps its UI font inside Data.hpak; the editor
            // reads it next to its executable.
            return VirtualFileSystem::GetDataRoot() / "Assets" / "Fonts" / InterFontFileName;
        }

        std::filesystem::path GetIconFontPath()
        {
            // Segoe MDL2 Assets is a Windows system font, so it is looked up under the Windows
            // directory rather than shipped with the editor. MergeIconFont falls back to text
            // glyphs when it is not installed.
            const char* windowsDirectory = std::getenv("WINDIR");
            const std::filesystem::path root = windowsDirectory != nullptr ? windowsDirectory : "C:/Windows";
            return root / "Fonts" / "segmdl2.ttf";
        }

        // Copies font bytes into memory owned by the ImGui atlas. ImGui frees the
        // allocation together with the atlas, so no buffer has to outlive this
        // call. Returns nullptr when the font is not available.
        void* LoadFontData(const std::filesystem::path& path, int& outSize)
        {
            outSize = 0;

            std::vector<uint8_t> bytes;
            if (!VirtualFileSystem::ReadBinaryFile(path, bytes) || bytes.empty())
            {
                return nullptr;
            }

            void* owned = IM_ALLOC(bytes.size());
            if (owned == nullptr)
            {
                return nullptr;
            }

            memcpy(owned, bytes.data(), bytes.size());
            outSize = static_cast<int>(bytes.size());
            return owned;
        }

        // Loads the Windows icon font into the default font so editor panels can use Segoe MDL2
        // glyphs (private use area) without shipping an extra icon font.
        void MergeIconFont(ImGuiIO& io, float pixelSize)
        {
            const std::filesystem::path iconFontPath = GetIconFontPath();
            int fontSize = 0;
            void* fontData = LoadFontData(iconFontPath, fontSize);
            if (fontData == nullptr)
            {
                HE_CORE_WARN("ImGui icon font not found at {}, icon glyphs will fall back to text", iconFontPath.string());
                return;
            }

            constexpr ImWchar IconFontRanges[] = { 0xE700, 0xF000, 0 };

            ImFontConfig iconConfig;
            iconConfig.MergeMode = true;
            iconConfig.PixelSnapH = true;
            iconConfig.GlyphMinAdvanceX = pixelSize;
            iconConfig.FontDataOwnedByAtlas = true;

            if (io.Fonts->AddFontFromMemoryTTF(fontData, fontSize, pixelSize, &iconConfig, IconFontRanges) == nullptr)
            {
                HE_CORE_WARN("Failed to parse ImGui icon font {}", iconFontPath.string());
            }
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
            int fontSize = 0;
            void* fontData = LoadFontData(fontPath, fontSize);
            if (fontData != nullptr)
            {
                ImFontConfig fontConfig;
                fontConfig.FontDataOwnedByAtlas = true;

                ImFont* interFont = io.Fonts->AddFontFromMemoryTTF(
                    fontData, fontSize, BaseFontSize * uiScale, &fontConfig);
                if (interFont != nullptr)
                {
                    io.FontDefault = interFont;
                    MergeIconFont(io, BaseFontSize * uiScale);
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
            MergeIconFont(io, BaseFontSize * uiScale);
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
