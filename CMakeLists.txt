cmake_minimum_required(VERSION 3.24)
project(WorldbuilderWriter VERSION 0.4.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

include(FetchContent)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(INSTALL_PROJECT OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    miniz
    GIT_REPOSITORY https://github.com/richgel999/miniz.git
    GIT_TAG 3.1.2
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(miniz)

find_package(Qt6 6.6 REQUIRED COMPONENTS Widgets PrintSupport Multimedia MultimediaWidgets Network Svg)
qt_standard_project_setup()

enable_testing()

qt_add_executable(WorldbuilderWriter
    src/main.cpp
    src/core/ArchiveDocument.cpp
    src/storage/ProjectStore.cpp
    src/storage/WbwPackage.cpp
    src/storage/GoogleDriveService.cpp
    src/import/ManuscriptImporter.cpp
    src/ui/ThemeManager.cpp
    src/ui/ProductReorganizer.cpp
    src/ui/NavigationConsistencyController.cpp
    src/ui/LibraryMockupController.cpp
    src/ui/MapMockupController.cpp
    src/ui/WritingMockupController.cpp
    src/ui/WritingShortcutBridge.cpp
    src/ui/WritingShortcutBridge.h
    src/ui/PlanningMockupController.cpp
    src/ui/ReviewMockupController.cpp
    src/ui/SettingsMockupController.cpp
    src/ui/SettingsWorkspace.cpp
    src/ui/SettingsWorkspace.h
    src/ui/RcVisualFixture.cpp
    src/ui/RcVisualFixture.h
    src/ui/RcVisualFixtureStartup.cpp
    src/ui/RelationshipBoard.cpp
    src/ui/MapExporterV2.cpp
    src/ui/MapExportDialog.cpp
    src/ui/PilinReyEditor.h
    src/ui/PilinReyEditorNative.cpp
    src/ui/ProjectHubPage.cpp
    src/ui/SettingsDialog.cpp
    src/ui/SemanticTextEdit.cpp
    src/ui/PlanningPage.cpp
    src/ui/WritingPage.cpp
    src/ui/WorldPage.cpp
    src/ui/WorldPageChromeV2.cpp
    src/ui/ReviewPage.cpp
    src/ui/ReviewPageChromeV2.cpp
    src/ui/MainWindow.cpp
)

target_include_directories(WorldbuilderWriter PRIVATE src)
target_link_libraries(WorldbuilderWriter PRIVATE
    Qt6::Widgets
    Qt6::PrintSupport
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::Network
    Qt6::Svg
    miniz
)

add_executable(WorldbuilderWriterCoreSmoke
    tests/CoreSmoke.cpp
    src/core/ArchiveDocument.cpp
    src/storage/WbwPackage.cpp
    src/import/ManuscriptImporter.cpp
)
target_include_directories(WorldbuilderWriterCoreSmoke PRIVATE src)
target_link_libraries(WorldbuilderWriterCoreSmoke PRIVATE Qt6::Widgets miniz)
add_test(NAME core-smoke COMMAND WorldbuilderWriterCoreSmoke)

add_executable(WorldbuilderWriterMapExporterSmoke
    tests/MapExporterSmoke.cpp
    src/ui/MapExporterV2.cpp
)
target_include_directories(WorldbuilderWriterMapExporterSmoke PRIVATE src)
target_link_libraries(WorldbuilderWriterMapExporterSmoke PRIVATE Qt6::Widgets Qt6::PrintSupport Qt6::Svg)
add_test(NAME map-exporter-smoke COMMAND WorldbuilderWriterMapExporterSmoke)

if(WIN32)
    set_target_properties(WorldbuilderWriter PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

install(TARGETS WorldbuilderWriter
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
qt_generate_deploy_app_script(
    TARGET WorldbuilderWriter
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
