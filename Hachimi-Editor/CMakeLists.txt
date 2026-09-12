# Hachimi-Editor console application.

set(EDITOR_SOURCES
    Source/EditorApplication.cpp
    Source/EditorApplication.h
    Source/Panels/BuildSettingsPanel.cpp
    Source/Panels/BuildSettingsPanel.h
    Source/Panels/ConsolePanel.cpp
    Source/Panels/ConsolePanel.h
    Source/Panels/ContentBrowserPanel.cpp
    Source/Panels/ContentBrowserPanel.h
    Source/Panels/EditorContext.h
    Source/Panels/EditorLayer.cpp
    Source/Panels/EditorLayer.h
    Source/Panels/EditorMenuBar.cpp
    Source/Panels/EditorMenuBar.h
    Source/Panels/GamePanel.cpp
    Source/Panels/GamePanel.h
    Source/Panels/InspectorPanel.cpp
    Source/Panels/InspectorPanel.h
    Source/Panels/PackageInspectorPanel.cpp
    Source/Panels/PackageInspectorPanel.h
    Source/Panels/ProjectHubLayer.cpp
    Source/Panels/ProjectHubLayer.h
    Source/Panels/SceneHierarchyPanel.cpp
    Source/Panels/SceneHierarchyPanel.h
    Source/Panels/ToolbarPanel.cpp
    Source/Panels/ToolbarPanel.h
    Source/Panels/ViewportPanel.cpp
    Source/Panels/ViewportPanel.h
    Source/UI/AssetBrowserGrid.cpp
    Source/UI/AssetBrowserGrid.h
    Source/UI/AssetPickerPopup.cpp
    Source/UI/AssetPickerPopup.h
    Source/Utils/FileDialogs.cpp
    Source/Utils/FileDialogs.h
    Source/Viewport/SelectionIndicators.cpp
    Source/Viewport/SelectionIndicators.h
)

add_executable(Hachimi-Editor ${EDITOR_SOURCES})

target_include_directories(Hachimi-Editor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

target_link_libraries(Hachimi-Editor
    PRIVATE
        Hachimi-Engine
        imguizmo::imguizmo
        nfd::nfd
        Ole32
        Shell32
        Uuid
)

set_common_compile_options(Hachimi-Editor)
add_dependencies(Hachimi-Editor copy_runtime_resources)
