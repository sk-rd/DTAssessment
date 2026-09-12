#pragma once

#include <fstream>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

#include "dta/damage_tolerance.hpp"
#include "dta/io/ndi.hpp"

namespace dta {

inline SimulationInput read_input_file(const std::string& path) {
    auto input = input_from_json(path);
    input.ndi = read_ndi_database(path, input.ndi);
    return input;
}

inline nlohmann::json output_to_json(const SimulationOutput& output) {
    nlohmann::json result{{"termination", output.termination}, {"history", nlohmann::json::array()}};
    for (const auto& state : output.history) {
        result["history"].push_back({
            {"cycles", state.cycles}, {"crack_length", state.crack_length},
            {"delta_k", state.delta_k}, {"max_k", state.max_k},
            {"growth_rate", std::isfinite(state.growth_rate)
                                 ? nlohmann::json(state.growth_rate)
                                 : nlohmann::json(nullptr)}});
    }
    return result;
}

inline void write_output_file(const std::string& path, const SimulationOutput& output) {
    std::ofstream file(path);
    if (!file) throw std::runtime_error("cannot open output file: " + path);
    file << output_to_json(output).dump(2) << '\n';
}

} // namespace dta
