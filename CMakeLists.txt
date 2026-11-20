cmake_minimum_required(VERSION 3.24)

project(LuluCode
    VERSION 0.1.0
    DESCRIPTION "Lulu Code - a local-first AI coding agent"
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(LULU_WITH_LLAMA "Build with the llama.cpp inference backend" ON)
option(LULU_FETCH_LLAMA "Fetch llama.cpp automatically when no local source is configured" ON)
option(LULU_BUILD_TESTS "Build Lulu Code unit tests" OFF)
set(LULU_LLAMA_TAG "b10333" CACHE STRING "llama.cpp release tag to fetch")
set(LULU_LLAMA_SOURCE_DIR "" CACHE PATH "Use a local llama.cpp checkout instead of fetching")

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# Qt 6
# ---------------------------------------------------------------------------
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Network Svg)
qt_standard_project_setup(REQUIRES 6.5)

# ---------------------------------------------------------------------------
# llama.cpp (static, trimmed down to the pieces we actually use)
# ---------------------------------------------------------------------------
if(LULU_WITH_LLAMA)
    if(LULU_LLAMA_SOURCE_DIR)
        message(STATUS "Lulu Code: using local llama.cpp at ${LULU_LLAMA_SOURCE_DIR}")
        set(LLAMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(LLAMA_BUILD_SERVER OFF CACHE BOOL "" FORCE)
        add_subdirectory(${LULU_LLAMA_SOURCE_DIR} ${CMAKE_BINARY_DIR}/llama.cpp EXCLUDE_FROM_ALL)
    elseif(LULU_FETCH_LLAMA)
        include(FetchContent)
        set(LLAMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(LLAMA_BUILD_SERVER OFF CACHE BOOL "" FORCE)
        set(LLAMA_CURL OFF CACHE BOOL "" FORCE)
        set(LLAMA_OPENSSL OFF CACHE BOOL "" FORCE)
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

        FetchContent_Declare(
            llama_cpp
            GIT_REPOSITORY https://github.com/ggml-org/llama.cpp.git
            GIT_TAG        ${LULU_LLAMA_TAG}
            GIT_SHALLOW    TRUE
        )
        FetchContent_MakeAvailable(llama_cpp)
        message(STATUS "Lulu Code: llama.cpp ${LULU_LLAMA_TAG} ready")
    else()
        message(FATAL_ERROR "LULU_WITH_LLAMA=ON but neither a local source tree nor FetchContent is available")
    endif()
endif()

# ---------------------------------------------------------------------------
# Application
# ---------------------------------------------------------------------------
set(LULU_SOURCES
    src/main.cpp

    src/app/AppPaths.cpp
    src/app/Logger.cpp

    src/core/ChatMessage.cpp
    src/core/ModelCatalog.cpp
    src/core/ModelStore.cpp
    src/core/LlamaEngine.cpp

    src/agent/ToolCall.cpp
    src/agent/ToolRunner.cpp
    src/agent/Toolchain.cpp
    src/agent/ProjectScanner.cpp
    src/agent/AgentSession.cpp

    src/ui/Theme.cpp
    src/ui/MainWindow.cpp
    src/ui/Sidebar.cpp
    src/ui/ChatPanel.cpp
    src/ui/MessageBubble.cpp
    src/ui/ToolCard.cpp
    src/ui/Composer.cpp
    src/ui/WelcomeView.cpp
    src/ui/WorkspacePanel.cpp
    src/ui/ModelsDialog.cpp
    src/ui/SettingsDialog.cpp
    src/ui/ApprovalDialog.cpp
)

set(LULU_HEADERS
    src/app/AppPaths.h
    src/app/Logger.h

    src/core/ChatMessage.h
    src/core/ModelCatalog.h
    src/core/ModelStore.h
    src/core/LlamaEngine.h

    src/agent/ToolCall.h
    src/agent/ToolRunner.h
    src/agent/Toolchain.h
    src/agent/ProjectScanner.h
    src/agent/AgentSession.h

    src/ui/Theme.h
    src/ui/MainWindow.h
    src/ui/Sidebar.h
    src/ui/ChatPanel.h
    src/ui/ElidedLabel.h
    src/ui/MessageBubble.h
    src/ui/ToolCard.h
    src/ui/Composer.h
    src/ui/WelcomeView.h
    src/ui/WorkspacePanel.h
    src/ui/ModelsDialog.h
    src/ui/SettingsDialog.h
    src/ui/ApprovalDialog.h
)

qt_add_executable(lulu_code ${LULU_SOURCES} ${LULU_HEADERS})

set_target_properties(lulu_code PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_BUNDLE_NAME "Lulu Code"
    MACOSX_BUNDLE_GUI_IDENTIFIER "dev.lulu.code"
    MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
)

target_include_directories(lulu_code PRIVATE src)

target_compile_definitions(lulu_code PRIVATE
    LULU_VERSION="${PROJECT_VERSION}"
    LULU_WITH_LLAMA=$<BOOL:${LULU_WITH_LLAMA}>
    QT_NO_CAST_FROM_ASCII
)

target_compile_options(lulu_code PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive- /utf-8>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>
)

target_link_libraries(lulu_code PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Svg
)

if(LULU_WITH_LLAMA)
    target_link_libraries(lulu_code PRIVATE llama)
endif()

qt_add_resources(lulu_code "lulu_assets"
    PREFIX "/"
    BASE "resources"
    FILES
        resources/theme.qss
        resources/icons/logo.png
        resources/icons/qwen.svg
        resources/icons/glm.svg
        resources/icons/deepseek.svg
        resources/icons/llama.svg
)

# Windows executable icon
if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources/icons/app.rc")
    target_sources(lulu_code PRIVATE resources/icons/app.rc)
endif()

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------
include(GNUInstallDirs)
install(TARGETS lulu_code
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(LULU_BUILD_TESTS)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()

message(STATUS "Lulu Code ${PROJECT_VERSION} configured (llama.cpp: ${LULU_WITH_LLAMA})")
