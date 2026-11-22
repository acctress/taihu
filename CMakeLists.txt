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

add_executable(taihu)

target_sources(taihu
        PRIVATE
        FILE_SET CXX_MODULES
        BASE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/source"
        FILES ${TAIHU_MODULES}
        PRIVATE
        source/entry.cpp
)

target_compile_definitions(taihu PRIVATE TAIHU_VERSION="${PROJECT_VERSION}")

target_link_libraries(taihu PRIVATE xiao::xiao)

set_target_properties(taihu PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/exe"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/lib"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/lib"
)
