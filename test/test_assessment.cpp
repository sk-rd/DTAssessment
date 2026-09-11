#include <boost/test/unit_test.hpp>

#include "dta/assessment.hpp"

#include <cmath>
#include <limits>

namespace {
dta::Assessment example() {
    dta::Assessment input;
    input.fleet_id = "fleet";
    input.trials = 4;
    input.max_cycles = 20;
    input.history_interval = 7;
    input.spectrum = {{100, 0, 3}, {120, 20, 2}};
    const dta::Component panel{"panel", {{"hole", "center_crack", .1, {}}}};
    input.airplanes = {{"plane", {{"wing", {panel}}}}};
    return input;
}
dta::Detail& site(dta::Assessment& input) {
    return input.airplanes[0].major_components[0].components[0].details[0];
}
const dta::Risk& risk(const dta::Result& result, const std::string& path) {
    for (const auto& r : result.risks)
        if (r.path == path) return r;
    throw std::runtime_error("Missing risk " + path);
}
}

BOOST_AUTO_TEST_CASE(analytical_geometry_database) {
    constexpr double pi = 3.14159265358979323846;
    BOOST_CHECK_CLOSE(dta::geometry_factor("center_crack", 0.01, 0.1),
                      std::sqrt(1 / std::cos(pi * 0.1)), 1e-10);
    BOOST_CHECK_CLOSE(dta::geometry_factor("edge_crack", 0.01, 0.1),
                      1.12 - .231 * .1 + 10.55 * .01 - 21.72 * .001 + 30.39 * .0001, 1e-10);
    BOOST_CHECK_THROW(dta::geometry_factor("center_crack", .05, .1), std::invalid_argument);
    BOOST_CHECK_THROW(dta::geometry_factor("edge_crack", .06, .1), std::invalid_argument);
    BOOST_CHECK_THROW(dta::geometry_factor("unknown", .01, .1), std::invalid_argument);
    BOOST_CHECK_THROW(dta::geometry_factor("center_crack", 0, .1), std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(seeded_reproducible_runout_and_history) {
    const auto input = example();
    const auto a = dta::assess(input);
    const auto b = dta::assess(input);
    BOOST_REQUIRE_EQUAL(a.representative_details.size(), 1U);
    BOOST_REQUIRE_EQUAL(a.risks.size(), 5U);
    const auto& dr = a.representative_details[0];
    BOOST_TEST(!dr.failed);
    BOOST_TEST(dr.failure_mode == "runout");
    BOOST_TEST(dr.life_cycles == input.max_cycles);
    BOOST_TEST(dr.final_size > dr.initial_size);
    BOOST_TEST(dr.initial_size == b.representative_details[0].initial_size);
    BOOST_TEST(dr.final_size == b.representative_details[0].final_size);
    BOOST_REQUIRE_EQUAL(dr.history.size(), 4U);
    BOOST_TEST(dr.history.front().cycle == 0U);
    BOOST_TEST(dr.history.back().cycle == input.max_cycles);
    BOOST_TEST(dr.history.back().crack_size == dr.final_size);
    BOOST_TEST(dr.history.back().residual_strength < dr.history.front().residual_strength);
    for (const auto& r : a.risks) BOOST_TEST(r.failed_trials == 0U);
    auto different = input;
    ++different.seed;
    BOOST_TEST(dr.initial_size != dta::assess(different).representative_details[0].initial_size);
}

BOOST_AUTO_TEST_CASE(threshold_and_zero_paris_coefficient_stop_growth) {
    auto input = example();
    input.material.threshold = 1e6;
    auto result = dta::assess(input);
    BOOST_TEST(result.representative_details[0].initial_size ==
               result.representative_details[0].final_size);
    input.material.threshold = 0;
    input.material.paris_c = 0;
    result = dta::assess(input);
    BOOST_TEST(result.representative_details[0].initial_size ==
               result.representative_details[0].final_size);
}

BOOST_AUTO_TEST_CASE(initial_failure_and_invalid_initial_geometry_are_events) {
    auto input = example();
    input.material.toughness = .001;
    auto result = dta::assess(input);
    BOOST_TEST(result.representative_details[0].failed);
    BOOST_TEST(result.representative_details[0].life_cycles == 0U);
    BOOST_TEST(result.representative_details[0].failure_mode == "toughness");
    BOOST_TEST(result.representative_details[0].history.size() == 1U);
    input.material.toughness = 40;
    site(input).initial = {1000, .08};
    BOOST_CHECK_NO_THROW(dta::validate(input));
    result = dta::assess(input);
    BOOST_TEST(result.representative_details[0].failure_mode == "ligament");
    BOOST_TEST(result.representative_details[0].history[0].residual_strength == 0);
    site(input).geometry = "edge_crack";
    result = dta::assess(input);
    BOOST_TEST(result.representative_details[0].failure_mode == "geometry_domain");
}

BOOST_AUTO_TEST_CASE(spectrum_peak_failure_and_repeated_blocks) {
    auto input = example();
    input.material.paris_c = 0;
    input.spectrum = {{1, 0, 2}, {1e6, 0, 1}};
    const auto result = dta::assess(input);
    const auto& dr = result.representative_details[0];
    BOOST_TEST(dr.failed);
    BOOST_TEST(dr.life_cycles == 3U);
    BOOST_TEST(dr.failure_mode == "toughness");
    BOOST_TEST(dr.history.back().cycle == 3U);
    BOOST_TEST(dr.initial_size == dr.final_size);
    input = example();
    input.trials = 1;
    input.max_cycles = 6;
    auto repeated = input;
    input.spectrum = {{100, 0, 1}, {110, 10, 1}};
    repeated.spectrum = {{100, 0, 1}, {110, 10, 1}, {100, 0, 1},
                         {110, 10, 1}, {100, 0, 1}, {110, 10, 1}};
    BOOST_TEST(dta::assess(input).representative_details[0].final_size ==
               dta::assess(repeated).representative_details[0].final_size);
}

BOOST_AUTO_TEST_CASE(paris_growth_failure_and_walker_compression_behavior) {
    auto input = example();
    input.trials = 1;
    site(input).initial = {1000, .001};
    input.material.paris_c = .01;
    auto result = dta::assess(input);
    BOOST_TEST(result.representative_details[0].failed);
    BOOST_TEST(result.representative_details[0].life_cycles == 1U);
    BOOST_TEST(result.representative_details[0].final_size >
               result.representative_details[0].initial_size);
    input.material.paris_c = 1e-8;
    input.spectrum = {{100, 0, 1}};
    const auto zero = dta::assess(input).representative_details[0].final_size;
    input.spectrum = {{100, -100, 1}};
    BOOST_TEST(dta::assess(input).representative_details[0].final_size == zero);
    input.spectrum = {{100, 50, 1}};
    const auto high_r = dta::assess(input).representative_details[0].final_size;
    BOOST_TEST(high_r < zero);
    input.material.walker_gamma = 0;
    BOOST_TEST(dta::assess(input).representative_details[0].final_size == zero);
}

BOOST_AUTO_TEST_CASE(one_cycle_paris_increment_and_numerical_safeguards) {
    auto input = example();
    input.trials = 1;
    input.max_cycles = 1;
    input.spectrum = {{100, 0, 1}};
    auto result = dta::assess(input);
    const auto& dr = result.representative_details[0];
    const double k = 100 * dta::geometry_factor("center_crack", dr.initial_size, .1) *
                     std::sqrt(3.14159265358979323846 * dr.initial_size);
    BOOST_CHECK_CLOSE(dr.final_size - dr.initial_size,
                      input.material.paris_c * std::pow(k, input.material.paris_m), 1e-6);
    input.material.paris_c = 1e-300;
    result = dta::assess(input);
    BOOST_TEST(result.representative_details[0].initial_size ==
               result.representative_details[0].final_size);
    input.material.paris_c = 1e308;
    BOOST_CHECK_THROW(dta::assess(input), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(paris_life_matches_analytical_wide_plate_solution) {
    auto input = example();
    input.trials = 1;
    input.max_cycles = 300;
    input.spectrum = {{100, 0, 1}};
    site(input).geometry = "edge_crack";
    site(input).width = 1e8;
    site(input).initial = {1000, .001};
    input.material.paris_c = 0;
    const double initial = dta::assess(input).representative_details[0].initial_size;
    constexpr double pi = 3.14159265358979323846;
    constexpr double y = 1.12;
    constexpr double critical_size = 1;
    constexpr double analytical_life = 200;
    // At a/W<=1e-8 the geometry is effectively the constant-Y wide-plate
    // limit. With m=2, a(N)=a0*exp(C*pi*(Y*sigma)^2*N) has an exact life.
    // About 3.5% growth/cycle also exercises fractional-growth substeps.
    input.material.paris_m = 2;
    input.material.paris_c = std::log(critical_size / initial) /
                             (analytical_life * pi * y * y * 100 * 100);
    input.material.toughness = 100 * y * std::sqrt(pi * critical_size);
    const auto result = dta::assess(input);
    const auto& dr = result.representative_details[0];
    BOOST_REQUIRE(dr.failed);
    BOOST_TEST(dr.failure_mode == "toughness");
    // Forward Euler delays exponential growth by <=~1% for 2% increments;
    // observed integer-cycle life adds at most one cycle of quantization.
    BOOST_TEST(dr.life_cycles >= analytical_life);
    BOOST_TEST(dr.life_cycles <= analytical_life * 1.011 + 1);
}

BOOST_AUTO_TEST_CASE(msd_med_are_grouped_cooccurrence_not_fleet_counts) {
    auto input = example();
    input.material.paris_c = 0;
    auto& major = input.airplanes[0].major_components[0];
    const dta::Detail failed{"a", "center_crack", .1, {1000, .08}};
    const dta::Detail safe{"b", "center_crack", .1, {1000, .001}};
    major.components = {{"one", {failed, safe}}, {"two", {failed, safe}}};
    auto result = dta::assess(input);
    BOOST_TEST(risk(result, "fleet/plane/wing").med_trials == input.trials);
    BOOST_TEST(risk(result, "fleet/plane/wing").msd_trials == 0U);
    BOOST_TEST(risk(result, "fleet").failed_trials == input.trials);
    BOOST_TEST(risk(result, "fleet").med_trials == input.trials);
    BOOST_TEST(risk(result, "fleet/plane/wing/one/b").failed_trials == 0U);
    major.components[0].details[1].initial = failed.initial;
    major.components[1].details[0].initial = safe.initial;
    result = dta::assess(input);
    BOOST_TEST(risk(result, "fleet/plane/wing/one").msd_trials == input.trials);
    BOOST_TEST(risk(result, "fleet/plane/wing").med_trials == 0U);
    BOOST_TEST(risk(result, "fleet").msd_trials == input.trials);
    BOOST_TEST(risk(result, "fleet/plane/wing/one/a").msd_trials == 0U);
    auto other_major = major;
    other_major.id = "tail";
    major.components.resize(1);
    other_major.components.resize(1);
    input.airplanes[0].major_components.push_back(other_major);
    result = dta::assess(input);
    BOOST_TEST(risk(result, "fleet").med_trials == 0U);
}

BOOST_AUTO_TEST_CASE(validation_rejects_bad_inputs_and_resource_requests) {
    auto good = example();
    auto bad = good;
    bad.material.paris_c = -1;
    BOOST_CHECK_THROW(dta::validate(bad), std::invalid_argument);
    bad = good; bad.material.walker_gamma = 1.1;
    BOOST_CHECK_THROW(dta::validate(bad), std::invalid_argument);
    bad = good; bad.material.toughness = std::numeric_limits<double>::infinity();
    BOOST_CHECK_THROW(dta::validate(bad), std::invalid_argument);
    bad = good; bad.spectrum[0].maximum = std::numeric_limits<double>::quiet_NaN();
    BOOST_CHECK_THROW(dta::validate(bad), std::invalid_argument);
    bad = good; bad.spectrum[0].minimum = bad.spectrum[0].maximum;
    BOOST_CHECK_THROW(dta::validate(bad), std::invalid_argument);
    bad = good; bad.spectrum[0].cycles = 0;
    BOOST_CHECK_THROW(dta::validate(bad), std::invalid_argument);
    bad = good; site(bad).initial.shape = 0;
    BOOST_CHECK_THROW(dta::validate(bad), std::invalid_argument);
    bad = good; site(bad).width = -1;
    BOOST_CHECK_THROW(dta::validate(bad), std::invalid_argument);
    bad = good; site(bad).geometry = "not_a_geometry";
    BOOST_CHECK_THROW(dta::validate(bad), std::invalid_argument);
    bad = good; bad.airplanes.clear();
    BOOST_CHECK_THROW(dta::validate(bad), std::invalid_argument);
    bad = good; site(bad).id = "../escape";
    BOOST_CHECK_THROW(dta::validate(bad), std::invalid_argument);
    bad = good; bad.airplanes.push_back(bad.airplanes[0]);
    BOOST_CHECK_THROW(dta::validate(bad), std::invalid_argument);
    bad = good; bad.airplanes[0].major_components[0].components[0].msd_min_failed_details = 1;
    BOOST_CHECK_THROW(dta::validate(bad), std::invalid_argument);
    bad = good; bad.airplanes[0].major_components[0].med_min_failed_components = 1;
    BOOST_CHECK_THROW(dta::validate(bad), std::invalid_argument);
    bad = good; bad.history_interval = 0;
    BOOST_CHECK_THROW(dta::validate(bad), std::invalid_argument);
    bad = good; bad.trials = 0;
    BOOST_CHECK_THROW(dta::validate(bad), std::invalid_argument);
    bad = good; bad.trials = 10000; bad.max_cycles = 1000000;
    BOOST_CHECK_THROW(dta::validate(bad), std::invalid_argument);
    bad = good; bad.trials = 1; bad.max_cycles = 1000000; bad.history_interval = 1;
    BOOST_CHECK_THROW(dta::validate(bad), std::invalid_argument);
}
