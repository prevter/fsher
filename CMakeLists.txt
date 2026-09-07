cmake_minimum_required(VERSION 3.21)
project(fsher LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(${PROJECT_NAME} STATIC)
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})

if (NOT TARGET fmt OR NOT TARGET GeodeResult OR NOT TARGET spirv-headers OR NOT TARGET spirv-tools OR NOT TARGET glslang OR NOT TARGET shaderc)
    if (NOT COMMAND CPMAddPackage)
        include(cmake/CPM.cmake)
    endif ()

    # Core
    if (NOT TARGET fmt)
        CPMAddPackage("gh:fmtlib/fmt#12.2.0")
    endif ()
    if (NOT TARGET GeodeResult)
        CPMAddPackage("gh:geode-sdk/result@1.4.1")
    endif ()

    # Vulkan
    if (NOT TARGET spirv-headers)
        CPMAddPackage("gh:KhronosGroup/SPIRV-Headers#vulkan-sdk-1.4.357.0")
    endif ()
    if (NOT TARGET spirv-tools)
        CPMAddPackage(
            URI "gh:KhronosGroup/SPIRV-Tools#vulkan-sdk-1.4.357.0"
            OPTIONS
                "SPIRV_SKIP_TESTS ON"
                "SPIRV_HEADER_INCLUDE_DIR ${SPIRV-Headers_SOURCE_DIR}/include"
        )
    endif ()
    if (NOT TARGET glslang)
        CPMAddPackage("gh:KhronosGroup/glslang#vulkan-sdk-1.4.357.0")
    endif ()
    if (NOT TARGET shaderc)
        CPMAddPackage(
            URI "gh:google/shaderc#v2026.3"
            OPTIONS
                "SHADERC_SKIP_TESTS ON"
                "SHADERC_SKIP_EXAMPLES ON"
                "SHADERC_SKIP_EXECUTABLES ON"
        )
    endif ()
endif ()

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "src/*.cpp")
target_sources(${PROJECT_NAME} PRIVATE ${SOURCES})

target_link_libraries(${PROJECT_NAME} PUBLIC fmt::fmt-header-only GeodeResult)
target_link_libraries(${PROJECT_NAME} PRIVATE shaderc)
target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

