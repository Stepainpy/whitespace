cmake_minimum_required(VERSION 3.21)
project(whitespace LANGUAGES C)

set(WS_LIB whitespace)
set(WS_EXE ws)
set(WS_TRL wst)

## External settings

option(WHITESPACE_BUILD_PROGRAM "Build executable program" ${PROJECT_IS_TOP_LEVEL})
option(WHITESPACE_BUILD_TESTS   "Build test cases"         ${PROJECT_IS_TOP_LEVEL})

if(DEFINED WHITESPACE_SHARED_LIBS)
    set(BUILD_SHARED_LIBS ${WHITESPACE_SHARED_LIBS})
endif()

## Setup flags

set(FLAGS -O2 -Wall -Wextra -Wpedantic)

if(MSVC)
    set(FLAGS /W4 /wd4267 /wd4244 /wd4146 /wd4457 /wd4456 /wd4701)
    if(CMAKE_BUILD_TYPE STREQUAL Debug)
        list(APPEND FLAGS /Od)
    else()
        list(APPEND FLAGS /O2)
    endif()
else()
    # For the future
    # list(APPEND FLAGS -Wno-long-long)
    list(APPEND FLAGS -Wswitch-enum)
endif()

if(CMAKE_C_COMPILER_ID MATCHES "Clang$")
    list(APPEND FLAGS -Wno-newline-eof)
endif()

## Create library

add_library(${WS_LIB})
add_library(ws::${WS_LIB} ALIAS ${WS_LIB})

set(LIB_HDRS
    include/whitespace/whitespace.h
)
set(LIB_SRCS
    src/defines.h
    src/label.h
    src/compile.c
    src/label.c
    src/disasm.c
    src/other.c
)

target_sources(${WS_LIB} PRIVATE ${LIB_SRCS})

set_target_properties(whitespace PROPERTIES
    C_STANDARD          90
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS        OFF
    C_VISIBILITY_PRESET hidden

    VISIBILITY_INLINES_HIDDEN ON

    PUBLIC_HEADER "${LIB_HDRS}"
)

target_compile_options(${WS_LIB} PRIVATE ${FLAGS})
target_include_directories(${WS_LIB}
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

## Create program

add_executable(${WS_EXE})

set(EXE_SRCS src/ws.c)

target_sources(${WS_EXE} PRIVATE ${EXE_SRCS})

set_target_properties(whitespace PROPERTIES
    C_STANDARD          90
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS        OFF
    C_VISIBILITY_PRESET hidden

    VISIBILITY_INLINES_HIDDEN ON
)

target_compile_options(${WS_EXE} PRIVATE ${FLAGS})
target_include_directories(${WS_EXE}
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(${WS_EXE} PRIVATE ${WS_LIB})

## Create translator

add_executable(${WS_TRL})

set(TRL_SRCS src/wst.c)

target_sources(${WS_TRL} PRIVATE ${TRL_SRCS})

set_target_properties(whitespace PROPERTIES
    C_STANDARD          90
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS        OFF
    C_VISIBILITY_PRESET hidden

    VISIBILITY_INLINES_HIDDEN ON
)

target_compile_options(${WS_TRL} PRIVATE ${FLAGS})
target_include_directories(${WS_TRL}
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

## Set output directories

if(PROJECT_IS_TOP_LEVEL)
    set_target_properties(${WS_LIB} PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    )
    set_target_properties(${WS_EXE} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    )
    set_target_properties(${WS_TRL} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    )
endif()
