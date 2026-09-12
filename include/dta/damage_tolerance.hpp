#pragma once

#include <algorithm>
#include <fstream>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "dta/crack_grow.hpp"
#include "dta/ndi.hpp"
#include "dta/io/spectrum.hpp"
namespace dta {

inline SimulationInput input_from_json(const std::string& filename) {
    std::ifstream file(filename);
    if (!file) throw std::runtime_error("cannot open input file: " + filename);
    nlohmann::json json;
    file >> json;
    SimulationInput input{json.at("material"), json.value("geometry", "center_crack"), json.at("width"),
                          json.at("initial_crack"), json.at("critical_crack"),
                          json.at("max_stress"), json.at("min_stress"),
                          json.at("cycles_per_step"), json.at("max_cycles"), {}, {}};
    const auto& ndi = json.at("NDI plan");
    input.ndi.name_zh = ndi.at("method");
    input.ndi.name_en = ndi.at("method");
    const auto read_optional = [](const nlohmann::json& object, const char* key) {
        return object.contains(key) ? object.at(key).get<double>() : -1.0;
    };
    input.ndi.threshold_cycles = read_optional(ndi.at("threshold"), "flight_cycles");
    input.ndi.threshold_hours = read_optional(ndi.at("threshold"), "flight_hours");
    input.ndi.interval_cycles = read_optional(ndi.at("interval"), "flight_cycles");
    input.ndi.interval_hours = read_optional(ndi.at("interval"), "flight_hours");
    if (input.ndi.threshold_cycles < 0.0 && input.ndi.threshold_hours < 0.0) {
        throw std::invalid_argument("NDI threshold requires flight cycles or flight hours");
    }
    if (input.ndi.interval_cycles < 0.0 && input.ndi.interval_hours < 0.0) {
        throw std::invalid_argument("NDI threshold and interval are required");
    }
    const auto spectrum_path =
        std::filesystem::path(filename).parent_path() / json.at("spectrum").get<std::string>();
    input.spectrum = read_spectrum_file(spectrum_path.string());
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
    double hours = 0.0;
    double crack_length = input.initial_crack;
    std::size_t spectrum_index = 0;
    double previous_time = -1.0;
    const double spectrum_duration = input.spectrum.points.back().time;
    while (true) {
        const auto& spectrum_point =
            input.spectrum.points[spectrum_index % input.spectrum.points.size()];
        const CrackLoad load{input.spectrum.reference_stress * spectrum_point.factor,
                             input.min_stress};
        const double dk = delta_k(geometry, load, crack_length);
        const double mk = max_k(geometry, load, crack_length);
        const double rate = growth_rate(material, geometry, load, crack_length);
        output.history.push_back({cycles, hours, crack_length, dk, mk, rate});
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
        const double elapsed_hours =
            previous_time < 0.0
                ? spectrum_point.time
                : (spectrum_point.time > previous_time
                       ? spectrum_point.time - previous_time
                       : spectrum_duration - previous_time + spectrum_point.time);
        hours += elapsed_hours;
        previous_time = spectrum_point.time;
        ++spectrum_index;
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
