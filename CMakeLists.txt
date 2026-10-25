cmake_minimum_required(VERSION 3.16)

project(PortScope
    VERSION 0.3.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
find_package(Threads REQUIRED)

add_library(portscope_lib
    src/cli.cpp
    src/port_parser.cpp
    src/scanner.cpp
    src/output.cpp
)

target_include_directories(portscope_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(portscope_lib PUBLIC Threads::Threads)

target_compile_options(portscope_lib PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

add_executable(portscope
    src/main.cpp
)

target_link_libraries(portscope PRIVATE portscope_lib)

target_compile_options(portscope PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

if(BUILD_TESTING)
    add_executable(portscope_tests
        tests/test_main.cpp
    )

    target_link_libraries(portscope_tests PRIVATE portscope_lib)

    target_compile_options(portscope_tests PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )

    add_test(NAME portscope_tests COMMAND portscope_tests)
endif()
