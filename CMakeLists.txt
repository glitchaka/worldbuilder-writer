cmake_minimum_required(VERSION 3.24)
project(WorldbuilderWriter VERSION 0.3.0 LANGUAGES C CXX)

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

find_package(Qt6 6.6 REQUIRED COMPONENTS Widgets PrintSupport Multimedia MultimediaWidgets)
qt_standard_project_setup()

qt_add_executable(WorldbuilderWriter
    src/main.cpp
    src/core/ArchiveDocument.cpp
    src/storage/ProjectStore.cpp
    src/storage/WbwPackage.cpp
    src/import/ManuscriptImporter.cpp
    src/ui/RelationshipBoard.cpp
    src/ui/MapCanvas.cpp
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
    miniz
)

if(WIN32)
    set_target_properties(WorldbuilderWriter PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

install(TARGETS WorldbuilderWriter BUNDLE DESTINATION . RUNTIME DESTINATION .)
qt_generate_deploy_app_script(TARGET WorldbuilderWriter OUTPUT_SCRIPT deploy_script NO_UNSUPPORTED_PLATFORM_ERROR)
install(SCRIPT ${deploy_script})
