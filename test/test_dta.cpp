#define BOOST_TEST_MODULE dta_tests
#include <boost/test/unit_test.hpp>

#include "dta/dta.hpp"

BOOST_AUTO_TEST_CASE(add_returns_sum) {
    BOOST_TEST(dta::add(2, 3) == 5);
}
