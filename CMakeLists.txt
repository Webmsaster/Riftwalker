cmake_minimum_required(VERSION 3.16)
project(Riftwalker VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
enable_testing()

# Find SDL2 packages (vcpkg CONFIG mode)
find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2_image CONFIG REQUIRED)
find_package(SDL2_mixer CONFIG REQUIRED)
find_package(SDL2_ttf CONFIG REQUIRED)

# Tracy Profiler (optional, enabled by default in Debug/RelWithDebInfo)
find_package(Tracy CONFIG REQUIRED)

# pixelmatch-cpp17 for visual regression testing
include(FetchContent)
FetchContent_Declare(
    pixelmatch-cpp17
    GIT_REPOSITORY https://github.com/jwmcglynn/pixelmatch-cpp17.git
    GIT_TAG v1.0.3
)

# Collect all source files
file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE HEADERS "src/*.h")

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

# MSVC: enable C++ exception handling + suppress common noise warnings
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /EHsc /wd4100 /wd4189)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    $<TARGET_NAME_IF_EXISTS:SDL2::SDL2main>
    $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
    $<IF:$<TARGET_EXISTS:SDL2_image::SDL2_image>,SDL2_image::SDL2_image,SDL2_image::SDL2_image-static>
    $<IF:$<TARGET_EXISTS:SDL2_mixer::SDL2_mixer>,SDL2_mixer::SDL2_mixer,SDL2_mixer::SDL2_mixer-static>
    $<IF:$<TARGET_EXISTS:SDL2_ttf::SDL2_ttf>,SDL2_ttf::SDL2_ttf,SDL2_ttf::SDL2_ttf-static>
    Tracy::TracyClient
)

# Copy assets to build directory
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
)

# Windows: copy SDL2 DLLs automatically
if(WIN32)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL2::SDL2>
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL2_image::SDL2_image>
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL2_mixer::SDL2_mixer>
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL2_ttf::SDL2_ttf>
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )

    find_program(RIFTWALKER_POWERSHELL NAMES powershell pwsh REQUIRED)
    add_test(
        NAME smoke_regression_short
        COMMAND "${RIFTWALKER_POWERSHELL}"
                -NoProfile
                -ExecutionPolicy Bypass
                -File "${CMAKE_SOURCE_DIR}/tools/smoke-regression.ps1"
                -Mode short
                -ExePath "$<TARGET_FILE:${PROJECT_NAME}>"
    )
    set_tests_properties(smoke_regression_short PROPERTIES LABELS "smoke;ci")
endif()

# Visual regression test tool
FetchContent_MakeAvailable(pixelmatch-cpp17)
add_executable(visual_diff tools/visual_diff.cpp src/Core/ScreenCapture.cpp)
target_include_directories(visual_diff PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(visual_diff PRIVATE
    $<IF:$<TARGET_EXISTS:SDL2::SDL2>,SDL2::SDL2,SDL2::SDL2-static>
    $<IF:$<TARGET_EXISTS:SDL2_image::SDL2_image>,SDL2_image::SDL2_image,SDL2_image::SDL2_image-static>
    pixelmatch-cpp17
)

# CTest: visual regression (compare against reference screenshots)
# Generate a launcher script at configure time to avoid quoting issues with spaces in paths
file(WRITE "${CMAKE_BINARY_DIR}/run_visual_regression.ps1" "
Set-StrictMode -Version Latest
& '${CMAKE_SOURCE_DIR}/tools/visual-regression.ps1' `
  -ExePath '${CMAKE_BINARY_DIR}/Release/Riftwalker.exe' `
  -DiffPath '${CMAKE_BINARY_DIR}/Release/visual_diff.exe' `
  -RefDir '${CMAKE_SOURCE_DIR}/tests/visual_reference'
exit $LASTEXITCODE
")
add_test(
    NAME visual_regression
    COMMAND "${RIFTWALKER_POWERSHELL}"
            -NoProfile -ExecutionPolicy Bypass
            -File "${CMAKE_BINARY_DIR}/run_visual_regression.ps1"
)
set_tests_properties(visual_regression PROPERTIES LABELS "visual;ci")
