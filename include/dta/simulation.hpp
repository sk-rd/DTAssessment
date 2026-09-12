#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <random>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "dta/sample.hpp"
#include "dta/ndi.hpp"

namespace dta {

struct SimulationConfig {
    std::size_t samples{1000};
    unsigned int seed{20260912};
    double stress_stddev_ratio{0.05};
    double initial_crack_stddev_ratio{0.05};
    double failure_probability_limit{0.01};
};

struct SimulationResult {
    std::size_t samples{};
    std::size_t failures{};
    double failure_probability{};
    double tenth_percentile_cycles{};
    double median_cycles{};
    double recommended_inspection_interval{};
    std::string recommendation;
    std::size_t detected_before_failure{};
};

class Simulation {
public:
    Simulation(const Material& material, const SimulationInput& nominal,
               SimulationConfig config = {}, NDI ndi = {})
        : material_(material), nominal_(nominal), config_(config), ndi_(std::move(ndi)) {}

    SimulationResult run() const {
        validate_config();
        std::mt19937 generator(config_.seed);
        std::normal_distribution<double> stress_factor(1.0, config_.stress_stddev_ratio);
        std::normal_distribution<double> crack_factor(1.0, config_.initial_crack_stddev_ratio);
        std::vector<double> lives;
        lives.reserve(config_.samples);
        std::size_t failures = 0;
        std::size_t detected_count = 0;

        for (std::size_t i = 0; i < config_.samples; ++i) {
            SimulationInput input = nominal_;
            input.max_stress = nominal_.max_stress * std::max(0.01, stress_factor(generator));
            input.initial_crack = std::min(
                nominal_.critical_crack * 0.999,
                nominal_.initial_crack * std::max(0.01, crack_factor(generator)));
            const auto result = Sample(material_, input).run();
            lives.push_back(result.history.back().cycles);
            bool detected = false;
            for (const auto& state : result.history) {
                if (state.cycles >= ndi_.start_cycles &&
                    std::fmod(state.cycles - ndi_.start_cycles, ndi_.interval_cycles) < 1e-9 &&
                    std::uniform_real_distribution<double>(0.0, 1.0)(generator) <=
                        ndi_.probability_of_detection(state.crack_length)) {
                    detected = true;
                    break;
                }
            }
            if (detected) ++detected_count;
            if (result.termination == "fracture" && !detected) ++failures;
        }

        std::sort(lives.begin(), lives.end());
        const auto percentile = [&lives](double fraction) {
            return lives[static_cast<std::size_t>(fraction * (lives.size() - 1))];
        };
        SimulationResult result{
            config_.samples,
            failures,
            static_cast<double>(failures) / static_cast<double>(config_.samples),
            percentile(0.10),
            percentile(0.50),
            std::max(1.0, percentile(0.10) / 2.0),
            {}};
        result.detected_before_failure = detected_count;
        result.recommendation =
            result.failure_probability > config_.failure_probability_limit
                ? "immediate inspection and shorter inspection interval"
                : "inspect at the recommended interval and continue monitoring";
        return result;
    }

private:
    void validate_config() const {
        if (config_.samples == 0 || config_.failure_probability_limit <= 0.0 ||
            config_.failure_probability_limit >= 1.0 ||
            config_.stress_stddev_ratio < 0.0 ||
            config_.initial_crack_stddev_ratio < 0.0) {
            throw std::invalid_argument("invalid simulation configuration");
        }
    }

    Material material_;
    SimulationInput nominal_;
    SimulationConfig config_;
    NDI ndi_;
};

} // namespace dta
