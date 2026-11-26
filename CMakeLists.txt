cmake_minimum_required(VERSION 3.20)
project(BifrostMini LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(BifrostMini
    src/main.cpp
    src/loa/game/io_backend.cpp
    src/loa/characters/base_character.cpp
    src/loa/identities/identities.cpp
    src/loa/characters/classes.cpp
    src/loa/interfaces/base_interface.cpp
    src/loa/interfaces/common_interfaces.cpp
    src/loa/interfaces/activity_interfaces.cpp
    src/loa/interfaces/exchange_mail_funnel.cpp
    src/loa/interfaces/stronghold_interfaces.cpp
    src/loa/interfaces/game_menu.cpp
    src/bf/vision/vision_engine.cpp
    src/bf/targetting/screen.cpp
    src/bf/targetting/minimap.cpp
    src/bf/targetting/path_monitoring.cpp
    src/bf/features/kurzan_front.cpp
    src/bf/features/guardian_raid.cpp
    src/bf/features/haals_hourglass.cpp
    src/bf/features/ebony_cube.cpp
    src/bf/features/paradise.cpp
    src/bf/features/off_the_hook.cpp
    src/bf/core/session.cpp
    src/bf/core/cycler_scheduler.cpp
    src/bf/streaming/stream_bridge.cpp
    src/bf/streaming/cloud_services.cpp
    src/bf/discord/discord_notifier.cpp
    src/bf/discord/discord_bot.cpp
    src/bf/settings/bifrost_settings.cpp
    src/bf/gui/account_panel.cpp
    src/bf/gui/roster_panels.cpp
    src/bf/gui/sidebar_pages.cpp
    src/bf/gui/ui_assets.cpp
    src/bf/gui/app_window.cpp
)

target_include_directories(BifrostMini PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if (MSVC)
    target_compile_options(BifrostMini PRIVATE /W4 /EHsc)
    target_link_options(BifrostMini PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
elseif (MINGW)
    target_link_options(BifrostMini PRIVATE -static -mwindows)
endif()

# Optional native SDK wiring (automatically enabled if found via vcpkg/CMAKE_PREFIX_PATH)
find_package(OpenCV QUIET)
if (OpenCV_FOUND)
    target_compile_definitions(BifrostMini PRIVATE BIFROST_HAS_OPENCV)
    target_link_libraries(BifrostMini PRIVATE ${OpenCV_LIBS})
endif()

find_package(Tesseract QUIET)
if (Tesseract_FOUND)
    target_compile_definitions(BifrostMini PRIVATE BIFROST_HAS_TESSERACT)
    target_link_libraries(BifrostMini PRIVATE Tesseract::libtesseract)
endif()

find_package(DarkHelp QUIET)
if (DarkHelp_FOUND)
    target_compile_definitions(BifrostMini PRIVATE BIFROST_HAS_DARKHELP)
    target_link_libraries(BifrostMini PRIVATE DarkHelp::DarkHelp)
endif()

if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/imgui.cpp")
    add_library(imgui_bundled STATIC
        third_party/imgui/imgui.cpp
        third_party/imgui/imgui_draw.cpp
        third_party/imgui/imgui_tables.cpp
        third_party/imgui/imgui_widgets.cpp
        third_party/imgui/backends/imgui_impl_win32.cpp
        third_party/imgui/backends/imgui_impl_dx11.cpp
    )
    target_include_directories(imgui_bundled PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui
        ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui/backends
    )
    target_compile_definitions(BifrostMini PRIVATE BIFROST_HAS_IMGUI)
    target_link_libraries(BifrostMini PRIVATE imgui_bundled d3d11 d3dcompiler dxgi dwmapi)
else()
    find_package(imgui QUIET)
    if (imgui_FOUND)
        target_compile_definitions(BifrostMini PRIVATE BIFROST_HAS_IMGUI)
        target_link_libraries(BifrostMini PRIVATE imgui::imgui d3d11 dxgi dwmapi)
    endif()
endif()

target_link_libraries(BifrostMini PRIVATE user32 gdi32 shell32 winhttp)

add_executable(layer0_tests
    tests/layer0_tests.cpp
    src/bf/targetting/screen.cpp
    src/bf/vision/vision_engine.cpp
    src/bf/streaming/stream_bridge.cpp
    src/loa/game/io_backend.cpp
    src/loa/interfaces/base_interface.cpp
)
target_include_directories(layer0_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(layer0_tests PRIVATE user32 gdi32 winmm)
if (OpenCV_FOUND)
    target_compile_definitions(layer0_tests PRIVATE BIFROST_HAS_OPENCV)
    target_link_libraries(layer0_tests PRIVATE ${OpenCV_LIBS})
endif()

add_executable(layer0_probe
    tools/layer0_probe.cpp
    src/bf/targetting/screen.cpp
    src/bf/vision/vision_engine.cpp
    src/bf/streaming/stream_bridge.cpp
    src/loa/game/io_backend.cpp
)
target_include_directories(layer0_probe PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(layer0_probe PRIVATE user32 gdi32)
if (OpenCV_FOUND)
    target_compile_definitions(layer0_probe PRIVATE BIFROST_HAS_OPENCV)
    target_link_libraries(layer0_probe PRIVATE ${OpenCV_LIBS})
endif()

add_executable(layer0_click_probe
    tools/layer0_click_probe.cpp
    src/bf/targetting/screen.cpp
    src/bf/vision/vision_engine.cpp
    src/bf/streaming/stream_bridge.cpp
    src/loa/game/io_backend.cpp
)
target_include_directories(layer0_click_probe PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(layer0_click_probe PRIVATE user32 gdi32)
if (OpenCV_FOUND)
    target_compile_definitions(layer0_click_probe PRIVATE BIFROST_HAS_OPENCV)
    target_link_libraries(layer0_click_probe PRIVATE ${OpenCV_LIBS})
endif()
