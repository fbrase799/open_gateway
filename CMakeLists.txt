cmake_minimum_required(VERSION 3.24)
project(open_gateway VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(OGW_BUILD_TESTS "Build tests" ON)
option(OGW_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

if(OGW_SANITIZERS)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined)
endif()

find_package(Boost 1.74 REQUIRED CONFIG)
find_package(Threads REQUIRED)

set(OGW_THIRD_PARTY "/opt/third_party" CACHE PATH "Preinstalled third-party sources")
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install OFF CACHE BOOL "" FORCE)

if(EXISTS "${OGW_THIRD_PARTY}/json/CMakeLists.txt")
    add_subdirectory("${OGW_THIRD_PARTY}/json" "${CMAKE_BINARY_DIR}/_deps/json" EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

function(ogw_warnings target)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror -Wpedantic)
endfunction()

function(ogw_plugin target source output_name)
    add_library(${target} SHARED ${source})
    target_link_libraries(${target} PRIVATE ogw_sdk Threads::Threads)
    target_compile_options(${target} PRIVATE -Wall -Wextra -Werror -Wpedantic -fvisibility=hidden)
    set_target_properties(${target} PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        PREFIX "lib"
        OUTPUT_NAME "${output_name}")
endfunction()

add_library(ogw_sdk INTERFACE)
target_include_directories(ogw_sdk INTERFACE "${CMAKE_SOURCE_DIR}/sdk/include")

add_library(ogw_core STATIC
    src/core/types.cpp
    src/core/clock.cpp
    src/core/log.cpp
    src/core/registry.cpp
    src/core/pipeline.cpp)
target_include_directories(ogw_core PUBLIC "${CMAKE_SOURCE_DIR}/include")
target_link_libraries(ogw_core PUBLIC Threads::Threads)
ogw_warnings(ogw_core)

add_library(ogw_host STATIC
    src/host/config.cpp
    src/host/descriptor.cpp
    src/host/plugin_manager.cpp)
target_include_directories(ogw_host PUBLIC "${CMAKE_SOURCE_DIR}/include" "${CMAKE_SOURCE_DIR}/sdk/include")
target_link_libraries(ogw_host PUBLIC ogw_core nlohmann_json::nlohmann_json ${CMAKE_DL_LIBS} Threads::Threads)
ogw_warnings(ogw_host)

add_library(ogw_codec STATIC src/codec/json_codec.cpp)
target_include_directories(ogw_codec PUBLIC "${CMAKE_SOURCE_DIR}/include")
target_link_libraries(ogw_codec PUBLIC ogw_core nlohmann_json::nlohmann_json)
ogw_warnings(ogw_codec)

add_library(ogw_api STATIC src/api/http_server.cpp src/api/mqtt_client.cpp)
target_include_directories(ogw_api PUBLIC "${CMAKE_SOURCE_DIR}/include")
target_link_libraries(ogw_api PUBLIC ogw_host ogw_codec Boost::headers Threads::Threads)
ogw_warnings(ogw_api)

add_library(ogw_app STATIC src/app/application.cpp)
target_link_libraries(ogw_app PUBLIC ogw_api)
ogw_warnings(ogw_app)

add_executable(ogw src/main.cpp)
target_link_libraries(ogw PRIVATE ogw_app)
ogw_warnings(ogw)

add_executable(ogw-driver src/driver/main.cpp)
target_link_libraries(ogw-driver PRIVATE ogw_app)
ogw_warnings(ogw-driver)

ogw_plugin(temperature_sim plugins/temperature_sim/temperature_sim.cpp temperature_sim)
ogw_plugin(bad_plugin plugins/bad_plugin/bad_plugin.cpp bad_plugin)

include(GNUInstallDirs)
install(TARGETS ogw ogw-driver RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY sdk/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(TARGETS temperature_sim bad_plugin LIBRARY DESTINATION examples)

if(OGW_BUILD_TESTS)
    set(CATCH_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
    set(CATCH_BUILD_TESTING OFF CACHE BOOL "" FORCE)
    if(EXISTS "${OGW_THIRD_PARTY}/Catch2/CMakeLists.txt")
        set(OGW_CATCH_DIR "${OGW_THIRD_PARTY}/Catch2")
        add_subdirectory("${OGW_CATCH_DIR}" "${CMAKE_BINARY_DIR}/_deps/Catch2" EXCLUDE_FROM_ALL)
    else()
        include(FetchContent)
        FetchContent_Declare(
            Catch2
            GIT_REPOSITORY https://github.com/catchorg/Catch2.git
            GIT_TAG v3.7.1
        )
        FetchContent_MakeAvailable(Catch2)
        set(OGW_CATCH_DIR "${catch2_SOURCE_DIR}")
    endif()

    ogw_plugin(ogw_test_plugin tests/fixtures/test_plugin.cpp ogw_test_plugin)
    add_library(ogw_abi_mismatch SHARED tests/fixtures/abi_mismatch.cpp)
    target_include_directories(ogw_abi_mismatch PRIVATE "${CMAKE_SOURCE_DIR}/sdk/include")
    target_compile_options(ogw_abi_mismatch PRIVATE -Wall -Wextra -Werror -Wpedantic -fvisibility=hidden)
    set_target_properties(ogw_abi_mismatch PROPERTIES CXX_VISIBILITY_PRESET hidden PREFIX "lib" OUTPUT_NAME "abi_mismatch")

    add_executable(ogw_unit_tests
        tests/unit/test_core.cpp
        tests/unit/test_plugins.cpp
        tests/unit/test_can_decode.cpp)
    target_include_directories(ogw_unit_tests PRIVATE "${CMAKE_SOURCE_DIR}/plugins/can_motor/include")
    target_link_libraries(ogw_unit_tests PRIVATE ogw_host ogw_core Catch2::Catch2WithMain)
    target_compile_definitions(ogw_unit_tests PRIVATE
        "OGW_TEST_PLUGIN_PATH=\"$<TARGET_FILE:ogw_test_plugin>\""
        "OGW_ABI_PLUGIN_PATH=\"$<TARGET_FILE:ogw_abi_mismatch>\"")
    ogw_warnings(ogw_unit_tests)
    add_dependencies(ogw_unit_tests ogw_test_plugin ogw_abi_mismatch)

    add_executable(ogw_live_tests tests/integration/live_test.cpp)
    target_link_libraries(ogw_live_tests PRIVATE Catch2::Catch2WithMain nlohmann_json::nlohmann_json Boost::headers Threads::Threads)
    target_include_directories(ogw_live_tests PRIVATE "${CMAKE_SOURCE_DIR}/sdk/include")
    ogw_warnings(ogw_live_tests)

    include(CTest)
    include("${OGW_CATCH_DIR}/extras/Catch.cmake")
    catch_discover_tests(ogw_unit_tests)
    add_test(NAME ogw_not_linked_to_plugins
        COMMAND ${CMAKE_COMMAND} -DBIN=$<TARGET_FILE:ogw> -P "${CMAKE_SOURCE_DIR}/cmake/AssertNoPluginLink.cmake")
    install(TARGETS ogw_live_tests RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
