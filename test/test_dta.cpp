#define BOOST_TEST_MODULE dta_tests
#include <boost/test/unit_test.hpp>

#include "dta/src.hpp"
BOOST_AUTO_TEST_CASE(crack_growth_round_trip) {
    const auto material = dta::read_material_file("../data/material/Al-7050-T7451.json");
    const auto input = dta::input_from_json("../data/input.json");
    BOOST_TEST(input.spectrum.reference_stress == 100.0);
    BOOST_TEST(input.spectrum.points.size() == 9);
    BOOST_TEST(input.spectrum.points.front().time == 0.0);
    BOOST_TEST(input.spectrum.points.back().time == 2.0);
    const auto output = dta::assess_damage_tolerance(material, input);
    BOOST_TEST(!output.history.empty());
    BOOST_TEST(output.history.front().crack_length == 0.01);
    BOOST_TEST(dta::output_to_json(output).at("history").size() == output.history.size());
}

BOOST_AUTO_TEST_CASE(monte_carlo_returns_inspection_advice) {
    const auto material = dta::read_material_file("../data/material/Al-7050-T7451.json");
    const auto input = dta::input_from_json("../data/input.json");
    dta::SimulationConfig config;
    config.samples = 20;
    const auto result = dta::Simulation(material, input, config).run();
    BOOST_TEST(result.samples == 20);
    BOOST_TEST(result.tenth_percentile_cycles >= 0.0);
    BOOST_TEST(!result.recommendation.empty());
}
