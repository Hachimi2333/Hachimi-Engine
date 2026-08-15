#include "Utils/FileDialogs.h"

#include <ImGuiFileDialog.h>

namespace HachimiEngine
{
    namespace
    {
        constexpr const char* ProjectDialogKey = "ProjectFileDialog";
        constexpr const char* DirectoryDialogKey = "DirectoryDialog";
        constexpr const char* TextureDialogKey = "TextureImportDialog";
        constexpr const char* SceneDialogKey = "SceneFileDialog";

        bool FinishDialog(const char* key, std::string& selectedPath)
        {
            if (!ImGuiFileDialog::Instance()->Display(key))
            {
                return false;
            }

            if (ImGuiFileDialog::Instance()->IsOk())
            {
                selectedPath = ImGuiFileDialog::Instance()->GetFilePathName();
            }

            ImGuiFileDialog::Instance()->Close();
            return true;
        }
    }

    void FileDialogs::OpenProjectFileDialog(const std::filesystem::path& startPath)
    {
        IGFD::FileDialogConfig config;
        config.path = startPath.string();
        config.flags = ImGuiFileDialogFlags_Default;
        ImGuiFileDialog::Instance()->OpenDialog(ProjectDialogKey, "Open Hachimi Project", ".hproj", config);
    }

    bool FileDialogs::DrawProjectFileDialog(std::string& selectedPath)
    {
        return FinishDialog(ProjectDialogKey, selectedPath);
    }

    void FileDialogs::OpenDirectoryDialog(const std::filesystem::path& startPath)
    {
        IGFD::FileDialogConfig config;
        config.path = startPath.string();
        config.flags = ImGuiFileDialogFlags_Default;
        ImGuiFileDialog::Instance()->OpenDialog(DirectoryDialogKey, "Choose Project Location", nullptr, config);
    }

    bool FileDialogs::DrawDirectoryDialog(std::string& selectedPath)
    {
        return FinishDialog(DirectoryDialogKey, selectedPath);
    }

    void FileDialogs::OpenTextureImportDialog(const std::filesystem::path& startPath)
    {
        IGFD::FileDialogConfig config;
        config.path = startPath.string();
        config.flags = ImGuiFileDialogFlags_Default;
        ImGuiFileDialog::Instance()->OpenDialog(TextureDialogKey, "Import Texture", "Image files (*.png *.jpg *.jpeg *.tga *.bmp){.png,.jpg,.jpeg,.tga,.bmp}", config);
    }

    bool FileDialogs::DrawTextureImportDialog(std::string& selectedPath)
    {
        return FinishDialog(TextureDialogKey, selectedPath);
    }

    void FileDialogs::OpenSceneFileDialog(const std::filesystem::path& startPath)
    {
        IGFD::FileDialogConfig config;
        config.path = startPath.string();
        config.flags = ImGuiFileDialogFlags_Default;
        ImGuiFileDialog::Instance()->OpenDialog(SceneDialogKey, "Open Scene", ".hscene", config);
    }

    bool FileDialogs::DrawSceneFileDialog(std::string& selectedPath)
    {
        return FinishDialog(SceneDialogKey, selectedPath);
    }
}
