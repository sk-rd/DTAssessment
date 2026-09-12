#define BOOST_TEST_MODULE dta_tests
#include <boost/test/unit_test.hpp>

#include "dta/src.hpp"
BOOST_AUTO_TEST_CASE(crack_growth_round_trip) {
    const auto material = dta::material_from_json("../data/material.json");
    const auto input = dta::input_from_json("../data/input.json");
    const auto output = dta::assess_damage_tolerance(material, input);
    BOOST_TEST(!output.history.empty());
    BOOST_TEST(output.history.front().crack_length == 0.01);
    BOOST_TEST(dta::output_to_json(output).at("history").size() == output.history.size());
}

BOOST_AUTO_TEST_CASE(monte_carlo_returns_inspection_advice) {
    const auto material = dta::material_from_json("../data/material.json");
    const auto input = dta::input_from_json("../data/input.json");
    dta::SimulationConfig config;
    config.samples = 20;
    const auto result = dta::Simulation(material, input, config).run();
    BOOST_TEST(result.samples == 20);
    BOOST_TEST(result.tenth_percentile_cycles >= 0.0);
    BOOST_TEST(!result.recommendation.empty());
}
