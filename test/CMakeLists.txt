add_executable(dta_tests
    test_dta.cpp
)

target_link_libraries(dta_tests
    PRIVATE
        dta
)

add_test(NAME dta_tests COMMAND dta_tests)
