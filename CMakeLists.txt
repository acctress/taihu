cmake_minimum_required(VERSION 3.21)

project(taihu VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (MSVC)
    add_compile_options(/MP)
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(TAIHU_XIAO_TAG "v0.1.0" CACHE STRING "xiao tag")
set(TAIHU_XIAO_DIR "" CACHE PATH "use this xiao checkout instead of fetching")

if (TAIHU_XIAO_DIR)
    add_subdirectory("${TAIHU_XIAO_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/xiao")
    message(STATUS "taihu: using xiao from ${TAIHU_XIAO_DIR}")
else()
    include(FetchContent)
    FetchContent_Declare(xiao
        GIT_REPOSITORY https://github.com/acctress/xiao.git
        GIT_TAG        ${TAIHU_XIAO_TAG}
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(xiao)
    message(STATUS "taihu: using xiao ${TAIHU_XIAO_TAG} from GitHub")
endif()

file(GLOB_RECURSE PROJ_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hh"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
)

file(GLOB_RECURSE PROJ_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cc"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cxx"
)

add_executable(taihu ${PROJ_SOURCES} ${PROJ_HEADERS})

target_compile_definitions(taihu PRIVATE TAIHU_VERSION="${PROJECT_VERSION}")

target_include_directories(taihu PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(taihu PRIVATE xiao::xiao)

set_target_properties(taihu PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/exe"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin/lib"
)
