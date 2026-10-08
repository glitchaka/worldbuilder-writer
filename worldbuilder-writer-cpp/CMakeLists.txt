cmake_minimum_required(VERSION 3.24)
project(WorldbuilderWriter VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.6 REQUIRED COMPONENTS Widgets PrintSupport)
qt_standard_project_setup()

qt_add_executable(WorldbuilderWriter
    src/main.cpp
    src/core/StoryProject.cpp
    src/storage/ProjectRepository.cpp
    src/ui/MainWindow.cpp
    src/ui/pages/ManuscriptPage.cpp
    src/ui/pages/CharactersPage.cpp
    src/ui/pages/WorldPage.cpp
    src/ui/pages/MagicPage.cpp
    src/ui/pages/TimelinePage.cpp
)

target_include_directories(WorldbuilderWriter PRIVATE src)
target_link_libraries(WorldbuilderWriter PRIVATE Qt6::Widgets Qt6::PrintSupport)

if (WIN32)
    set_target_properties(WorldbuilderWriter PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

install(TARGETS WorldbuilderWriter
    BUNDLE DESTINATION .
    RUNTIME DESTINATION .
)

qt_generate_deploy_app_script(
    TARGET WorldbuilderWriter
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})
