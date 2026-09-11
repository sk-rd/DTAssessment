add_executable(dta_tests
    test_dta.cpp
)

target_link_libraries(dta_tests
    PRIVATE
        dta
)

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
