#pragma once

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "dta/crack_grow.hpp"
#include "dta/ndi.hpp"
namespace dta {

inline SimulationInput input_from_json(const std::string& filename) {
    std::ifstream file(filename);
    if (!file) throw std::runtime_error("cannot open input file: " + filename);
    nlohmann::json json;
    file >> json;
    SimulationInput input{json.value("geometry", "center_crack"), json.at("width"),
                          json.at("initial_crack"), json.at("critical_crack"),
                          json.at("max_stress"), json.at("min_stress"),
                          json.at("cycles_per_step"), json.at("max_cycles"), {}};
    const auto& ndi = json.at("ndi");
    input.ndi.name_zh = ndi.at("name_zh");
    input.ndi.name_en = ndi.at("name_en");
    input.ndi.start_cycles = ndi.at("start_cycles");
    input.ndi.interval_cycles = ndi.at("interval_cycles");
    for (const auto& point : ndi.at("pod")) {
        input.ndi.pod.push_back({point.at("crack_length"), point.at("probability")});
    }
    if (input.ndi.interval_cycles <= 0.0 || input.ndi.pod.empty()) {
        throw std::invalid_argument("NDI interval and POD curve are required");
    }
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

        inline SimulationOutput CrackGrow::run() const {
            return assess_damage_tolerance(material_, input_);
        }
        const double step = std::min(input.cycles_per_step, input.max_cycles - cycles);
        crack_length = std::min(input.critical_crack, crack_length + rate * step);
        cycles += step;
    }
    return output;
}

struct ReliabilityResult {
    double failure_probability{};
    double median_cycles{};
    double recommended_inspection_interval{};
};

class DamageTolerance {
public:
    template <typename Result>
    static ReliabilityResult analyze(const Result& result) {
        return {result.failure_probability, result.median_cycles,
                result.recommended_inspection_interval};
    }
};

} // namespace dta
