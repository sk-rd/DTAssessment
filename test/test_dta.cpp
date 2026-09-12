#define BOOST_TEST_MODULE dta_tests
#include <boost/test/unit_test.hpp>

#include "dta/dta.hpp"
BOOST_AUTO_TEST_CASE(crack_growth_round_trip) {
    const auto material = dta::material_from_json("../data/material.json");
    const auto input = dta::input_from_json("../data/input.json");
    const auto output = dta::assess_damage_tolerance(material, input);
    BOOST_TEST(!output.history.empty());
    BOOST_TEST(output.history.front().crack_length == 0.01);
    BOOST_TEST(dta::output_to_json(output).at("history").size() == output.history.size());
}
