add_library(downspout_counterpointer_core STATIC
    src/counterpointer_engine.cpp
    src/counterpointer_transport.cpp
)
target_link_libraries(downspout_counterpointer_core PUBLIC downspout-project-options)
target_include_directories(downspout_counterpointer_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

add_library(downspout_counterpointer_port INTERFACE)
target_link_libraries(downspout_counterpointer_port INTERFACE
    downspout-project-options
    downspout_counterpointer_core
)

if(BUILD_TESTING)
    add_executable(downspout_counterpointer_core_tests
        tests/counterpointer_core_tests.cpp
    )
    target_link_libraries(downspout_counterpointer_core_tests PRIVATE downspout_counterpointer_core)
    add_test(NAME downspout_counterpointer_core_tests COMMAND downspout_counterpointer_core_tests)
endif()
