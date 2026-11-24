cmake_minimum_required(VERSION 4.5)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_MODULE_STD ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(taihu VERSION 0.1.0 LANGUAGES CXX)

if (MSVC)
    add_compile_options(/MP)
endif ()

if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-Wno-include-angled-in-module-purview)
endif ()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(TAIHU_XIAO_TAG "v0.1.0" CACHE STRING "xiao tag")
set(TAIHU_XIAO_DIR "" CACHE PATH "use this xiao checkout instead of fetching")

if (TAIHU_XIAO_DIR)
    add_subdirectory("${TAIHU_XIAO_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/xiao")
    message(STATUS "taihu: using xiao from ${TAIHU_XIAO_DIR}")
else ()
    include(FetchContent)
    FetchContent_Declare(xiao
            GIT_REPOSITORY https://github.com/acctress/xiao.git
            GIT_TAG ${TAIHU_XIAO_TAG}
            GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(xiao)
    message(STATUS "taihu: using xiao ${TAIHU_XIAO_TAG} from GitHub")
endif ()

file(GLOB_RECURSE TAIHU_MODULES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cppm")

add_library(taihu_core STATIC)
target_sources(taihu_core PUBLIC
        FILE_SET CXX_MODULES BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/source" FILES ${TAIHU_MODULES})
target_link_libraries(taihu_core PUBLIC xiao::xiao)

add_executable(taihu source/entry.cpp)
target_link_libraries(taihu PRIVATE taihu_core)

target_compile_definitions(taihu PRIVATE TAIHU_VERSION="${PROJECT_VERSION}")

set_target_properties(taihu PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/exe"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/lib"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/lib"
)

option(TAIHU_TESTS "build tests" ${PROJECT_IS_TOP_LEVEL})
if (TAIHU_TESTS)
    include(FetchContent)
    FetchContent_Declare(catch2
            URL https://github.com/catchorg/Catch2/archive/refs/tags/v3.7.1.tar.gz
            URL_HASH SHA256=c991b247a1a0d7bb9c39aa35faf0fe9e19764213f28ffba3109388e62ee0269c)
    FetchContent_MakeAvailable(catch2)

    file(GLOB TAIHU_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")
    add_executable(taihu_tests ${TAIHU_TEST_SOURCES})
    target_link_libraries(taihu_tests PRIVATE taihu_core Catch2::Catch2WithMain)

    enable_testing()
    include("${catch2_SOURCE_DIR}/extras/Catch.cmake")
    catch_discover_tests(taihu_tests)

    add_test(NAME driver.preprocess
            COMMAND taihu -E -DLEVEL=3 -DNAME=name "${CMAKE_CURRENT_SOURCE_DIR}/tests/c/macros.c")
    set_tests_properties(driver.preprocess PROPERTIES PASS_REGULAR_EXPRESSION
            "# 7 \"[^\"]*macros.c\"\r?\nint area = 640 \\* 480.\r?\nint level = 3.\r?\nint self = 0.\r?\n\r?\n\r?\nint width = WIDTH.\r?\nconst char\\* name = name.")
endif ()