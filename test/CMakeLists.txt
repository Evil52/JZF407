cmake_minimum_required(VERSION 3.22)

# Standalone host-side test project. Does NOT use ARM toolchain.
# Build:  cmake -B build_test -S test
#         cmake --build build_test
#         ctest --test-dir build_test --output-on-failure

project(jtf407_tests C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

enable_testing()

# Unity framework
add_library(unity STATIC unity/unity.c)
target_include_directories(unity PUBLIC unity)

# Common test settings — strict warnings, define TEST_BUILD for production code
function(add_jtf407_test name source)
    add_executable(${name} ${source} mocks/mocks.c ${ARGN})
    target_compile_definitions(${name} PRIVATE TEST_BUILD=1)
    target_include_directories(${name} PRIVATE
        .
        mocks
        ../Core/Inc
    )
    target_link_libraries(${name} PRIVATE unity)
    if(MSVC)
        target_compile_options(${name} PRIVATE /W3 /wd4204 /wd4127)
    else()
        target_compile_options(${name} PRIVATE -Wall -Wextra)
    endif()
    add_test(NAME ${name} COMMAND ${name})
endfunction()

# fault_marker: MARKER_ADDR redirected to mock_ccmram via TEST_BUILD guard
add_jtf407_test(test_fault_marker test_fault_marker.c ../Core/Src/fault_marker.c)

# led_dispatch is pure C, no hardware deps
add_jtf407_test(test_led_dispatch test_led_dispatch.c ../Core/Src/led_dispatch.c)

# debouncer is pure C, no hardware deps
add_jtf407_test(test_debouncer test_debouncer.c ../Core/Src/debouncer.c)
