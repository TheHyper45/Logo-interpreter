cmake_minimum_required(VERSION 3.8)
if("${CMAKE_SOURCE_DIR}" STREQUAL "${CMAKE_BINARY_DIR}")
	message(FATAL_ERROR "In-source builds are not allowed.")
endif()

set(CMAKE_CXX_STANDARD 20)
#set(CMAKE_CXX_STANDARD_REQUIRED TRUE)

project(LogoInterpreter VERSION 0 LANGUAGES CXX)

add_executable(logo code/main.cpp code/utils.hpp code/string.hpp code/string.cpp)

set_target_properties(logo PROPERTIES LINKER_LANGUAGE CXX)
target_compile_definitions(logo PRIVATE "$<$<CONFIG:DEBUG>:DEBUG_BUILD>")

if(MSVC)
    set_property(DIRECTORY ${CMAKE_BINARY_DIR} PROPERTY VS_STARTUP_PROJECT logo)
    target_compile_options(logo PRIVATE /utf-8 /W4 /Wall /MP /permissive- /Zc:preprocessor /wd4820 /wd4514 /wd4626 /wd5045 /wd5264)
else()
    target_compile_options(logo PRIVATE -Wall -Wextra -Wshadow -Wconversion -pedantic)
endif()
