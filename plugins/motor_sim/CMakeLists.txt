cmake_minimum_required(VERSION 3.24)
project(motor_sim LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT OGW_SDK_DIR)
    message(FATAL_ERROR "Set OGW_SDK_DIR to the installed Open Gateway SDK prefix")
endif()

find_package(Threads REQUIRED)

add_library(motor_sim SHARED src/motor_sim.cpp)
target_include_directories(motor_sim PRIVATE "${OGW_SDK_DIR}/include")
target_compile_options(motor_sim PRIVATE -Wall -Wextra -Werror -Wpedantic -fvisibility=hidden)
target_link_libraries(motor_sim PRIVATE Threads::Threads)
set_target_properties(motor_sim PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    PREFIX "lib"
    OUTPUT_NAME "motor_sim")
