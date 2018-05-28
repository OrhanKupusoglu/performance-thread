cmake_minimum_required(VERSION 3.5)

set(BUILD_NAME prf-system)

set(BUILD_MAJOR_VER 0)
set(BUILD_MINOR_VER 3)
set(BUILD_PATCH_VER 1)

set(SOURCE_FILES src/prf_system.c)

set(HEADER_FILES include/prf_system.h)

project(${BUILD_NAME} VERSION ${BUILD_MAJOR_VER}.${BUILD_MINOR_VER}.${BUILD_PATCH_VER} LANGUAGES C)

add_library(${BUILD_NAME} STATIC ${SOURCE_FILES})

target_compile_options(${BUILD_NAME} INTERFACE -Wall
                                               -Wextra
                                               -Wpedantic
)

target_include_directories(${BUILD_NAME} PUBLIC include)
set_target_properties(${BUILD_NAME} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/lib)
install(FILES ${HEADER_FILES} DESTINATION ${PROJECT_SOURCE_DIR}/lib)
