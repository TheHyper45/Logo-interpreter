cmake_minimum_required(VERSION 3.8)
if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
	message(FATAL_ERROR "In-source builds are not allowed.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

project(LogoInterpreter VERSION 0 LANGUAGES CXX)

add_executable(logo
               code/main.cpp
               code/utils.hpp
               code/string.hpp
               code/string.cpp
               code/debug.hpp
               code/debug.cpp
               code/array_view.hpp
               code/heap_array.hpp
               code/static_array.hpp
               code/lexer.hpp
               code/lexer.cpp
               code/parser.hpp
               code/parser.cpp
               code/memory_arena.hpp
               code/memory_arena.cpp)

set_target_properties(logo PROPERTIES LINKER_LANGUAGE CXX)
target_compile_definitions(logo PRIVATE "$<$<CONFIG:DEBUG>:DEBUG_BUILD>")

if(MSVC)
    set_property(DIRECTORY ${CMAKE_BINARY_DIR} PROPERTY VS_STARTUP_PROJECT logo)
    set_target_properties(logo PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:logo>"
                                          VS_DEBUGGER_COMMAND "$<TARGET_FILE:logo>"
                                          VS_DEBUGGER_ENVIRONMENT "PATH=%PATH%;${CMAKE_PREFIX_PATH}/bin")
    target_compile_options(logo PRIVATE /utf-8 /W4 /Wall /MP /permissive- /Zc:preprocessor /EHs-c- /D_HAS_EXCEPTIONS=0 /GR- /wd4820 /wd4514 /wd4626 /wd5045 /wd5264 /wd4623 /wd4061)
else()
    target_compile_options(logo PRIVATE -Wall -Wextra -Wshadow -Wconversion -pedantic -fno-exceptions -fno-rtti)
endif()
