#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "dta/damage_tolerance.hpp"

namespace dta {

struct MonteCarloConfig {
    std::size_t samples{1000};
    unsigned int seed{20260912};
    double stress_stddev_ratio{0.05};
    double initial_crack_stddev_ratio{0.05};
    double failure_probability_limit{0.01};
};

struct MonteCarloResult {
    std::size_t samples{};
    std::size_t failures{};
    double failure_probability{};
    double tenth_percentile_cycles{};
    double median_cycles{};
    double recommended_inspection_interval{};
    std::string recommendation;
};

inline MonteCarloResult run_monte_carlo(const Material& material,
                                        const SimulationInput& nominal,
                                        const MonteCarloConfig& config = {}) {
    if (config.samples == 0 || config.failure_probability_limit <= 0.0 ||
        config.failure_probability_limit >= 1.0 ||
        config.stress_stddev_ratio < 0.0 || config.initial_crack_stddev_ratio < 0.0) {
        throw std::invalid_argument("invalid Monte Carlo configuration");
    }

    std::mt19937 generator(config.seed);
    std::normal_distribution<double> stress_factor(1.0, config.stress_stddev_ratio);
    std::normal_distribution<double> crack_factor(1.0, config.initial_crack_stddev_ratio);
    std::vector<double> failure_cycles;
    failure_cycles.reserve(config.samples);
    std::size_t failures = 0;

    for (std::size_t i = 0; i < config.samples; ++i) {
        SimulationInput sample = nominal;
        sample.max_stress = nominal.max_stress * std::max(0.01, stress_factor(generator));
        sample.initial_crack = nominal.initial_crack *
                              std::max(0.01, crack_factor(generator));
        sample.initial_crack = std::min(sample.initial_crack, sample.critical_crack * 0.999);
        const auto result = assess_damage_tolerance(material, sample);
        const double life = result.history.back().cycles;
        failure_cycles.push_back(life);
        if (result.termination == "fracture") {
            ++failures;
        }
    }

    std::sort(failure_cycles.begin(), failure_cycles.end());
    const auto percentile = [&failure_cycles](double fraction) {
        const auto index = static_cast<std::size_t>(
            fraction * static_cast<double>(failure_cycles.size() - 1));
        return failure_cycles[index];
    };
    MonteCarloResult output{
        config.samples,
        failures,
        static_cast<double>(failures) / static_cast<double>(config.samples),
        percentile(0.10),
        percentile(0.50),
        std::max(1.0, percentile(0.10) / 2.0),
        {}};
    if (output.failure_probability > config.failure_probability_limit) {
        output.recommendation = "immediate inspection and shorter inspection interval";
    } else {
        output.recommendation = "inspect at the recommended interval and continue monitoring";
    }
    return output;
}

inline nlohmann::json simulation_to_json(const MonteCarloResult& result) {
    return {
        {"samples", result.samples},
        {"failures", result.failures},
        {"failure_probability", result.failure_probability},
        {"tenth_percentile_cycles", result.tenth_percentile_cycles},
        {"median_cycles", result.median_cycles},
        {"recommended_inspection_interval", result.recommended_inspection_interval},
        {"recommendation", result.recommendation}};
}

} // namespace dta
