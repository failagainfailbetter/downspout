add_executable(downspout-ai-coordinator
    src/main.cpp
    src/midi_file_writer.cpp
    src/solo_generator.cpp
    src/state_json.cpp
)
target_link_libraries(downspout-ai-coordinator PRIVATE
    downspout-project-options
    downspout_sidecar_core
)
target_include_directories(downspout-ai-coordinator PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

if(BUILD_TESTING)
    add_executable(downspout_ai_coordinator_tests
        tests/ai_coordinator_tests.cpp
        src/midi_file_writer.cpp
        src/solo_generator.cpp
        src/state_json.cpp
    )
    target_link_libraries(downspout_ai_coordinator_tests PRIVATE
        downspout-project-options
        downspout_sidecar_core
    )
    target_include_directories(downspout_ai_coordinator_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
    )
    add_test(NAME downspout_ai_coordinator_tests COMMAND downspout_ai_coordinator_tests)
endif()
