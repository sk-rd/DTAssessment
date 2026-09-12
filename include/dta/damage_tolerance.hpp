#pragma once

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "dta/crack_growth.hpp"

namespace dta {

struct SimulationInput {
    std::string geometry{"center_crack"};
    double width{}, initial_crack{}, critical_crack{};
    double max_stress{}, min_stress{}, cycles_per_step{}, max_cycles{};
};

struct SimulationOutput {
    std::string termination;
    std::vector<CrackState> history;
};

inline SimulationInput input_from_json(const std::string& filename) {
    std::ifstream file(filename);
    if (!file) throw std::runtime_error("cannot open input file: " + filename);
    nlohmann::json json;
    file >> json;
    SimulationInput input{json.value("geometry", "center_crack"), json.at("width"),
                          json.at("initial_crack"), json.at("critical_crack"),
                          json.at("max_stress"), json.at("min_stress"),
                          json.at("cycles_per_step"), json.at("max_cycles")};
    if (input.geometry != "center_crack" && input.geometry != "edge_crack") {
        throw std::invalid_argument("geometry must be center_crack or edge_crack");
    }
    require_positive("width", input.width);
    require_positive("initial_crack", input.initial_crack);
    require_positive("critical_crack", input.critical_crack);
    require_positive("cycles_per_step", input.cycles_per_step);
    require_positive("max_cycles", input.max_cycles);
    if (input.initial_crack >= input.critical_crack ||
        input.critical_crack >= input.width / 2.0) {
        throw std::invalid_argument("crack lengths must satisfy 0 < initial_crack < critical_crack < width/2");
    }
    if (!(input.max_stress > input.min_stress) || !std::isfinite(input.max_stress) ||
        !std::isfinite(input.min_stress)) {
        throw std::invalid_argument("max_stress must exceed finite min_stress");
    }
    return input;
}

inline SimulationOutput assess_damage_tolerance(const Material& material,
                                                const SimulationInput& input) {
    const CrackGeometry geometry{input.geometry, input.width};
    const CrackLoad load{input.max_stress, input.min_stress};
    SimulationOutput output;
    double cycles = 0.0;
    double crack_length = input.initial_crack;
    while (true) {
        const double dk = delta_k(geometry, load, crack_length);
        const double mk = max_k(geometry, load, crack_length);
        const double rate = growth_rate(material, geometry, load, crack_length);
        output.history.push_back({cycles, crack_length, dk, mk, rate});
        if (mk >= material.fracture_toughness || crack_length >= input.critical_crack) {
            output.termination = "fracture";
            break;
        }
        if (cycles >= input.max_cycles) {
            output.termination = "max_cycles";
            break;
        }
        const double step = std::min(input.cycles_per_step, input.max_cycles - cycles);
        crack_length = std::min(input.critical_crack, crack_length + rate * step);
        cycles += step;
    }
    return output;
}

} // namespace dta
