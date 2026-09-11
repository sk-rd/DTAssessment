add_executable(dta_tests
    test_dta.cpp
)

target_link_libraries(dta_tests
    PRIVATE
        dta
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(dta_tests)
