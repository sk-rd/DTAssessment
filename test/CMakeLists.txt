find_package(Boost CONFIG REQUIRED COMPONENTS unit_test_framework)

add_executable(dta_tests
    test_dta.cpp
    test_assessment.cpp
    test_json.cpp
)

target_link_libraries(dta_tests
    PRIVATE
        Boost::unit_test_framework
        dta
        nlohmann_json::nlohmann_json
)

add_test(NAME dta_tests COMMAND dta_tests)
target_compile_definitions(dta_tests PRIVATE DTA_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

if(DTA_BUILD_CLI)
    add_test(NAME dta_cli_example
        COMMAND dta_assess "${PROJECT_SOURCE_DIR}/data/example.json"
                           "${CMAKE_CURRENT_BINARY_DIR}/assessment-result.json")
endif()

if(DTA_BUILD_PYTHON_BINDINGS)
    if(DTA_BUILD_CLI)
        set(DTA_PYTHON_CLI_ARGUMENT "$<TARGET_FILE:dta_assess>")
    endif()
    add_test(
        NAME dta_python_tests
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_dta.py ${DTA_PYTHON_CLI_ARGUMENT}
    )
    set_tests_properties(dta_python_tests PROPERTIES
        ENVIRONMENT "PYTHONPATH=$<TARGET_FILE_DIR:dta_python>"
    )
endif()
