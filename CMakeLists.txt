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

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.4.18
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(SDL3)

find_package(Qt6 6.6 REQUIRED COMPONENTS Widgets PrintSupport Multimedia MultimediaWidgets Network)
qt_standard_project_setup()

enable_testing()

qt_add_executable(WorldbuilderWriter
    src/main.cpp
    src/core/ArchiveDocument.cpp
    src/storage/ProjectStore.cpp
    src/storage/WbwPackage.cpp
    src/storage/GoogleDriveService.cpp
    src/import/ManuscriptImporter.cpp
    src/ui/RelationshipBoard.cpp
    src/ui/MapExporter.cpp
    src/ui/MapExportDialog.cpp
    src/ui/PilinReyEditor.cpp
    src/ui/ProjectHubPage.cpp
    src/ui/SettingsDialog.cpp
    src/ui/PlanningPage.cpp
    src/ui/WritingPage.cpp
    src/ui/WorldPage.cpp
    src/ui/ReviewPage.cpp
    src/ui/MainWindow.cpp
)

target_include_directories(WorldbuilderWriter PRIVATE src)
target_link_libraries(WorldbuilderWriter PRIVATE
    Qt6::Widgets
    Qt6::PrintSupport
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::Network
    SDL3::SDL3
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
    src/ui/MapExporter.cpp
)
target_include_directories(WorldbuilderWriterMapExporterSmoke PRIVATE src)
target_link_libraries(WorldbuilderWriterMapExporterSmoke PRIVATE Qt6::Widgets Qt6::PrintSupport)
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
