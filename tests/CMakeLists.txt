# Lulu Code unit tests

cmake_minimum_required(VERSION 3.24)

enable_testing()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Test Network)

# Collect test sources from tests/ directory
file(GLOB test_sources "tests/**/*test*.cpp" "tests/**/*Test*.cpp")

if(test_sources)
    add_lulu_code_tests
    target_sources(lulu_code_tests PRIVATE ${test_sources})
    target_link_libraries(lulu_code_tests PRIVATE
        Qt6::Test
        Qt6::Core
    )
    
    # Register the test executable with ctest
    add_dependencies(lulu_code_tests lulu_code)
else()
    message(STATUS "No test files found in tests/ directory")
endif()

# Helper macro to create a test that runs in offscreen mode
macro(create_offscreen_test test_name source_file)
    add_test(NAME ${test_name} COMMAND lulu_code)
    set_tests_properties(${test_name} PROPERTIES
        ENVIRONMENT
            QT_QPA_PLATFORM offscreen
    )
endmacro()