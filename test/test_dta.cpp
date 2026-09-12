#define BOOST_TEST_MODULE dta_tests
#include <boost/test/unit_test.hpp>

#include "dta/dta.hpp"
#include <nlohmann/json.hpp>

BOOST_AUTO_TEST_CASE(crack_growth_round_trip) {
    const auto material = dta::material_from_json({
        {"c", 1e-10}, {"m", 3.0}, {"threshold_delta_k", 1.0}, {"fracture_toughness", 80.0}});
    const auto input = dta::input_from_json({
        {"geometry", "center_crack"}, {"width", 1.0}, {"initial_crack", 0.01},
        {"critical_crack", 0.2}, {"max_stress", 100.0}, {"min_stress", 0.0},
        {"cycles_per_step", 1000.0}, {"max_cycles", 10000.0}});
    const auto output = dta::assess_damage_tolerance(material, input);
    BOOST_TEST(!output.history.empty());
    BOOST_TEST(output.history.front().crack_length == 0.01);
    BOOST_TEST(dta::output_to_json(output).at("history").size() == output.history.size());
}
