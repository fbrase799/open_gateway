cmake_minimum_required(VERSION 3.24)
project(can_motor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT OGW_SDK_DIR)
    message(FATAL_ERROR "Set OGW_SDK_DIR to the installed Open Gateway SDK prefix")
endif()

find_package(Threads REQUIRED)

add_library(can_motor SHARED src/can_motor.cpp)
target_include_directories(can_motor PRIVATE "${OGW_SDK_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_options(can_motor PRIVATE -Wall -Wextra -Werror -Wpedantic -fvisibility=hidden)
target_link_libraries(can_motor PRIVATE Threads::Threads)
set_target_properties(can_motor PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    PREFIX "lib"
    OUTPUT_NAME "can_motor")
