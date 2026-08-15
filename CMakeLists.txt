cmake_minimum_required(VERSION 3.21)
project(whitespace LANGUAGES C)

## External settings

option(WHITESPACE_BUILD_PROGRAM "Build executable program" ${PROJECT_IS_TOP_LEVEL})
option(WHITESPACE_BUILD_TESTS   "Build test cases"         ${PROJECT_IS_TOP_LEVEL})

if(DEFINED WHITESPACE_SHARED_LIBS)
    set(BUILD_SHARED_LIBS ${WHITESPACE_SHARED_LIBS})
endif()

## Declare library and programs

set(WS_LIB wsvm)
set(WS_EXE ws  )
set(WS_TRL wstl)
set(WS_DIS wsda)

add_library(${WS_LIB})
add_library(whitespace::${WS_LIB} ALIAS ${WS_LIB})

add_executable(${WS_EXE})
add_executable(${WS_TRL})
add_executable(${WS_DIS})

## Setup flags and settings

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

function(setup_target TGT)
    # Compile settings
    target_compile_options(${TGT} PRIVATE ${FLAGS})
    set_target_properties(${TGT} PROPERTIES
        C_STANDARD          90
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS        OFF
        C_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
    )

    # Directories visibility
    target_include_directories(${TGT}
        PUBLIC
            "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
            "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>"
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )

    # Output directories
    if(PROJECT_IS_TOP_LEVEL)
        set_target_properties(${TGT} PROPERTIES
            ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
        )
    endif()
endfunction()

setup_target(${WS_LIB})
setup_target(${WS_EXE})
setup_target(${WS_TRL})
setup_target(${WS_DIS})

## Create library

set(LIB_HDR
    include/whitespace/whitespace.h
)
set(LIB_SRC
    src/code.h
    src/array.h
    src/label.h
    src/parse.h

    src/compile.c
    src/execute.c
    src/disasm.c
    src/error.c
    src/other.c

    src/label.c
    src/parse.c
)

target_sources(${WS_LIB} PRIVATE ${LIB_SRC})
set_target_properties(${WS_LIB} PROPERTIES PUBLIC_HEADER "${LIB_HDR}")

## Create program

target_sources(${WS_EXE} PRIVATE src/ws.c)
target_link_libraries(${WS_EXE} PRIVATE ${WS_LIB})

## Create translator

target_sources(${WS_TRL} PRIVATE src/wstl.c)

## Create disassembler

target_sources(${WS_DIS} PRIVATE src/wsda.c)
target_link_libraries(${WS_DIS} PRIVATE ${WS_LIB})
