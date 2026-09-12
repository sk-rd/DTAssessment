find_package(Boost CONFIG REQUIRED COMPONENTS unit_test_framework)

add_executable(dta_tests
    test_dta.cpp
)

target_link_libraries(dta_tests
    PRIVATE
        Boost::unit_test_framework
        dta
)

target_link_libraries(dta_tests PRIVATE nlohmann_json::nlohmann_json)

add_test(NAME dta_tests COMMAND dta_tests)

if(DTA_BUILD_PYTHON_BINDINGS)
    add_test(
        NAME dta_python_tests
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/test_dta.py
    )
    set_tests_properties(dta_python_tests PROPERTIES
        ENVIRONMENT "PYTHONPATH=$<TARGET_FILE_DIR:dta_python>"
    )
endif()

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(
    NAME dta_python_script_tests
    COMMAND ${Python3_EXECUTABLE} -c
        "import sys; sys.path.insert(0, '${CMAKE_CURRENT_SOURCE_DIR}/../scripts'); import dta_simulator"
)
