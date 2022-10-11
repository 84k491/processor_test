cmake_minimum_required(VERSION 3.10)

set(CMAKE_C_COMPILER "clang")
set(CMAKE_CXX_COMPILER "clang++")

project(Processor VERSION 1.0)

include_directories(${PROJECT_SOURCE_DIR})

file(GLOB processor_SRC "*.cpp")
add_executable(Processor ${processor_SRC})
target_compile_options(Processor PRIVATE "-g" "-Og" "-Wall" "-Wpedantic" "-Werror" "-Wextra")

set_target_properties(Processor PROPERTIES CXX_STANDARD 17)

target_link_libraries(Processor -lpthread)
