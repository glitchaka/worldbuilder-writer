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

find_package(Qt6 6.6 REQUIRED COMPONENTS Widgets PrintSupport Multimedia MultimediaWidgets Network Svg)
qt_standard_project_setup()

enable_testing()

set(WBW_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${WBW_GENERATED_DIR}")

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/PilinReyEditorV4.cpp" WBW_PILIN_V4)
string(REPLACE
    "for (QWidget* w : {static_cast<QWidget*>(layersButton_), assetsButton_, templateButton_, appearanceButton_, fitButton, undoButton_, redoButton_, exportButton}) commands->addWidget(w);"
    "commands->addWidget(layersButton_); commands->addWidget(assetsButton_); commands->addWidget(templateButton_); commands->addWidget(appearanceButton_); commands->addWidget(fitButton); commands->addWidget(undoButton_); commands->addWidget(redoButton_); commands->addWidget(exportButton);"
    WBW_PILIN_V4 "${WBW_PILIN_V4}")
string(REPLACE
    "int tw = 1, th = 1; SDL_GetTextureSize(texture, reinterpret_cast<float*>(&tw), reinterpret_cast<float*>(&th));\n            const float aspect = 1.0f;"
    "float tw = 1.0f, th = 1.0f; SDL_GetTextureSize(texture, &tw, &th);\n            const float aspect = th > 0.0f ? tw / th : 1.0f;"
    WBW_PILIN_V4 "${WBW_PILIN_V4}")
string(REPLACE
    "setMinimumSize(720, 460);"
    "setMinimumSize(320, 220);"
    WBW_PILIN_V4 "${WBW_PILIN_V4}")
string(REPLACE
    "zoom_ = std::clamp(std::min((width() - 60.0) / docW, (height() - 60.0) / docH), 0.04, 5.0);"
    "zoom_ = std::clamp(std::min((width() - 24.0) / docW, (height() - 24.0) / docH), 0.04, 5.0);"
    WBW_PILIN_V4 "${WBW_PILIN_V4}")
string(REPLACE
    "viewInitialized_ = true;\n        renderFrame();"
    "viewInitialized_ = true; autoFit_ = true;\n        renderFrame();"
    WBW_PILIN_V4 "${WBW_PILIN_V4}")
string(REPLACE
    "void resizeEvent(QResizeEvent* event) override { QWidget::resizeEvent(event); renderFrame(); }"
    "void resizeEvent(QResizeEvent* event) override { QWidget::resizeEvent(event); if (autoFit_) fitDocument(); else renderFrame(); }"
    WBW_PILIN_V4 "${WBW_PILIN_V4}")
string(REPLACE
    "void wheelEvent(QWheelEvent* event) override {\n        const QPointF before"
    "void wheelEvent(QWheelEvent* event) override {\n        autoFit_ = false;\n        const QPointF before"
    WBW_PILIN_V4 "${WBW_PILIN_V4}")
string(REPLACE
    "if (event->button() == Qt::MiddleButton || tool_ == Tool::Pan) {\n            panning_ = true;"
    "if (event->button() == Qt::MiddleButton || tool_ == Tool::Pan) {\n            autoFit_ = false;\n            panning_ = true;"
    WBW_PILIN_V4 "${WBW_PILIN_V4}")
string(REPLACE
    "bool viewInitialized_ = false;\n    bool snap_ = false;"
    "bool viewInitialized_ = false;\n    bool autoFit_ = true;\n    bool snap_ = false;"
    WBW_PILIN_V4 "${WBW_PILIN_V4}")
string(REPLACE
    "SDL_SetRenderDrawColor(renderer_, 38, 40, 44, 255); SDL_RenderClear(renderer_);"
    "SDL_SetRenderDrawColor(renderer_, 18, 19, 21, 255); SDL_RenderClear(renderer_);"
    WBW_PILIN_V4 "${WBW_PILIN_V4}")
string(REPLACE
    "if (++col == 2) { col = 0; ++row; }"
    "if (++col == 1) { col = 0; ++row; }"
    WBW_PILIN_V4 "${WBW_PILIN_V4}")
string(REPLACE
    "auto* options = new QWidget(toolRail_); auto* optionLayout = new QVBoxLayout(options);"
    "auto* options = new QFrame(canvasHost_); options->setObjectName(QStringLiteral(\"pilinToolOptions\")); auto* optionLayout = new QVBoxLayout(options);"
    WBW_PILIN_V4 "${WBW_PILIN_V4}")
string(REPLACE
    "tools->addWidget(options, row + 1, 0, 1, 2);"
    "options->hide();"
    WBW_PILIN_V4 "${WBW_PILIN_V4}")
string(REPLACE
    "if (toolRail_) { toolRail_->adjustSize(); toolRail_->move(m, m); toolRail_->raise(); }"
    "if (toolRail_) { toolRail_->adjustSize(); toolRail_->move(m, m); toolRail_->raise(); } if (auto* options = canvasHost_->findChild<QWidget*>(QStringLiteral(\"pilinToolOptions\")); options && options->isVisible()) { options->adjustSize(); options->move(m + (toolRail_ ? toolRail_->width() : 0) + 8, m); options->raise(); }"
    WBW_PILIN_V4 "${WBW_PILIN_V4}")
string(REPLACE
    "viewport_->setTool(tool); toolOptionsLabel_"
    "viewport_->setTool(tool); if (auto* options = canvasHost_->findChild<QWidget*>(QStringLiteral(\"pilinToolOptions\"))) { const bool needsOptions = tool == Tool::Coast || tool == Tool::Eraser || tool == Tool::Forest || tool == Tool::Mountain || tool == Tool::River || tool == Tool::Road || tool == Tool::Border || tool == Tool::Region; options->setVisible(needsOptions); } layoutFloatingPanels(); toolOptionsLabel_"
    WBW_PILIN_V4 "${WBW_PILIN_V4}")
file(WRITE "${WBW_GENERATED_DIR}/PilinReyEditorV4.cpp" "${WBW_PILIN_V4}")

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/MapExporterV2.cpp" WBW_MAP_EXPORTER_V2)
string(REPLACE
    "const int count=std::max(24,pts.size()*density/2);"
    "const int count=std::max(24,static_cast<int>(pts.size())*density/2);"
    WBW_MAP_EXPORTER_V2 "${WBW_MAP_EXPORTER_V2}")
string(REPLACE
    "const int count=std::max(18,pts.size()*density/3);"
    "const int count=std::max(18,static_cast<int>(pts.size())*density/3);"
    WBW_MAP_EXPORTER_V2 "${WBW_MAP_EXPORTER_V2}")
string(REPLACE
    "const double t=static_cast<double>(i)/std::max(1,poly.size()-1);"
    "const double t=static_cast<double>(i)/std::max(1,static_cast<int>(poly.size())-1);"
    WBW_MAP_EXPORTER_V2 "${WBW_MAP_EXPORTER_V2}")
file(WRITE "${WBW_GENERATED_DIR}/MapExporterV2.cpp" "${WBW_MAP_EXPORTER_V2}")

qt_add_executable(WorldbuilderWriter
    src/main.cpp
    src/core/ArchiveDocument.cpp
    src/storage/ProjectStore.cpp
    src/storage/WbwPackage.cpp
    src/storage/GoogleDriveService.cpp
    src/import/ManuscriptImporter.cpp
    src/ui/ThemeManager.cpp
    src/ui/ProductReorganizer.cpp
    src/ui/MapMockupController.cpp
    src/ui/WritingMockupController.cpp
    src/ui/RelationshipBoard.cpp
    "${WBW_GENERATED_DIR}/MapExporterV2.cpp"
    src/ui/MapExportDialog.cpp
    src/ui/PilinReyEditor.h
    "${WBW_GENERATED_DIR}/PilinReyEditorV4.cpp"
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
    "${WBW_GENERATED_DIR}/MapExporterV2.cpp"
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
