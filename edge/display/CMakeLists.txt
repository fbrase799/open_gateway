cmake_minimum_required(VERSION 3.24)
project(ogw_display LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Network Quick Qml)

qt_add_executable(ogw-display
    src/main.cpp
    src/catalog.cpp
    src/display_config.cpp
    src/GatewayClient.cpp
    src/GatewayClient.hpp
    src/catalog.hpp
    src/display_config.hpp)
qt_add_resources(ogw-display "qml" PREFIX "/" FILES qml/Main.qml)
target_include_directories(ogw-display PRIVATE src)
target_link_libraries(ogw-display PRIVATE Qt6::Core Qt6::Network Qt6::Quick Qt6::Qml)
target_compile_options(ogw-display PRIVATE -Wall -Wextra -Werror)

qt_add_executable(ogw-display-tests
    tests/test_catalog.cpp
    src/catalog.cpp
    src/display_config.cpp
    src/catalog.hpp
    src/display_config.hpp)
target_include_directories(ogw-display-tests PRIVATE src)
target_link_libraries(ogw-display-tests PRIVATE Qt6::Core)
target_compile_options(ogw-display-tests PRIVATE -Wall -Wextra -Werror)

enable_testing()
add_test(NAME catalog COMMAND ogw-display-tests)
