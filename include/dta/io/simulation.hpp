#pragma once

#include <fstream>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

#include "dta/simulation.hpp"

namespace dta {

inline nlohmann::json simulation_to_json(const SimulationResult& result) {
    return {
        {"samples", result.samples},
        {"failures", result.failures},
        {"failure_probability", result.failure_probability},
        {"tenth_percentile_cycles", result.tenth_percentile_cycles},
        {"median_cycles", result.median_cycles},
        {"recommended_inspection_interval", result.recommended_inspection_interval},
        {"detected_before_failure", result.detected_before_failure},
        {"recommendation", result.recommendation}};
}

inline void write_simulation_file(const std::string& path, const SimulationResult& result) {
    std::ofstream file(path);
    if (!file) throw std::runtime_error("cannot open simulation output file: " + path);
    file << simulation_to_json(result).dump(2) << '\n';
}

} // namespace dta
