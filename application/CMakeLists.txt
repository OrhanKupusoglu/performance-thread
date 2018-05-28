cmake_minimum_required(VERSION 3.5)

set(BUILD_NAME prf-system-app)

set(BUILD_MAJOR_VER 0)
set(BUILD_MINOR_VER 3)
set(BUILD_PATCH_VER 1)

set(SOURCE_FILES src/main.c)

set(HEADER_FILES prf_system.h)

set(CMAKE_LIBRARY_PATH ${PROJECT_SOURCE_DIR}/../library/lib)

project(${BUILD_NAME} VERSION ${BUILD_MAJOR_VER}.${BUILD_MINOR_VER}.${BUILD_PATCH_VER} LANGUAGES C)

add_executable(${BUILD_NAME} ${SOURCE_FILES})

target_compile_options(${BUILD_NAME} INTERFACE -Wall
                                               -Wextra
                                               -Wpedantic
)

find_library(LIB_PERF NAMES prf-system PATHS ${PROJECT_SOURCE_DIR}/../library PATH_SUFFIXES lib)

execute_process(COMMAND dirname -- ${LIB_PERF} OUTPUT_VARIABLE LIB_DIR)
string(REGEX REPLACE "\n$" "" LIB_DIR ${LIB_DIR})

message(STATUS "Link library      : ${LIB_PERF}")
message(STATUS "Include directory : ${LIB_DIR}")

target_link_libraries(${BUILD_NAME} PRIVATE -pthread)
target_link_libraries(${BUILD_NAME} PUBLIC ${LIB_PERF})
target_include_directories(${BUILD_NAME} PUBLIC ${LIB_DIR})
set_target_properties(${BUILD_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)
