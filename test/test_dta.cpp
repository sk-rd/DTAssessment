#include <gtest/gtest.h>

#include "dta/dta.hpp"

TEST(DtaTest, AddReturnsSum) {
    EXPECT_EQ(dta::add(2, 3), 5);
}
