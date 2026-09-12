#pragma once

#include <cmath>
#include <algorithm>
#include <cstddef>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace dta {

constexpr double pi = 3.14159265358979323846;

struct Material {
    double c{};
    double m{};
    double threshold_delta_k{};
    double fracture_toughness{};
};

struct SimulationInput {
    std::string geometry{"center_crack"};
    double width{};
    double initial_crack{};
    double critical_crack{};
    double max_stress{};
    double min_stress{};
    double cycles_per_step{};
    double max_cycles{};
};

struct CrackState {
    double cycles{};
    double crack_length{};
    double delta_k{};
    double max_k{};
    double growth_rate{};
};

struct SimulationOutput {
    std::string termination;
    std::vector<CrackState> history;
};

inline void require_positive(const char* name, double value) {
    if (!(value > 0.0) || !std::isfinite(value)) {
        throw std::invalid_argument(std::string(name) + " must be positive and finite");
    }
}

inline Material material_from_json(const nlohmann::json& json) {
    Material material{
        json.at("c").get<double>(),
        json.at("m").get<double>(),
        json.at("threshold_delta_k").get<double>(),
        json.at("fracture_toughness").get<double>()};
    require_positive("c", material.c);
    require_positive("m", material.m);
    require_positive("threshold_delta_k", material.threshold_delta_k);
    require_positive("fracture_toughness", material.fracture_toughness);
    return material;
}

inline SimulationInput input_from_json(const nlohmann::json& json) {
    SimulationInput input{
        json.value("geometry", "center_crack"),
        json.at("width").get<double>(),
        json.at("initial_crack").get<double>(),
        json.at("critical_crack").get<double>(),
        json.at("max_stress").get<double>(),
        json.at("min_stress").get<double>(),
        json.at("cycles_per_step").get<double>(),
        json.at("max_cycles").get<double>()};
    if (input.geometry != "center_crack" && input.geometry != "edge_crack") {
        throw std::invalid_argument("geometry must be center_crack or edge_crack");
    }
    require_positive("width", input.width);
    require_positive("initial_crack", input.initial_crack);
    require_positive("critical_crack", input.critical_crack);
    require_positive("cycles_per_step", input.cycles_per_step);
    require_positive("max_cycles", input.max_cycles);
    if (input.initial_crack >= input.critical_crack || input.critical_crack >= input.width / 2.0) {
        throw std::invalid_argument("crack lengths must satisfy 0 < initial_crack < critical_crack < width/2");
    }
    if (!(input.max_stress > input.min_stress) || !std::isfinite(input.max_stress) ||
        !std::isfinite(input.min_stress)) {
        throw std::invalid_argument("max_stress must exceed finite min_stress");
    }
    return input;
}

inline double geometry_factor(const SimulationInput& input, double crack_length) {
    if (input.geometry == "edge_crack") {
        return 1.12;
    }
    return std::sqrt(1.0 / std::cos(pi * crack_length / input.width));
}

inline double delta_k(const SimulationInput& input, double crack_length) {
    return geometry_factor(input, crack_length) * (input.max_stress - input.min_stress) *
           std::sqrt(pi * crack_length);
}

inline double max_k(const SimulationInput& input, double crack_length) {
    return geometry_factor(input, crack_length) * input.max_stress * std::sqrt(pi * crack_length);
}

inline double growth_rate(const Material& material, const SimulationInput& input, double crack_length) {
    const double delta_k_value = delta_k(input, crack_length);
    if (delta_k_value <= material.threshold_delta_k) {
        return 0.0;
    }
    if (max_k(input, crack_length) >= material.fracture_toughness) {
        return std::numeric_limits<double>::infinity();
    }
    return material.c * std::pow(delta_k_value, material.m);
}

inline SimulationOutput simulate(const Material& material, const SimulationInput& input) {
    SimulationOutput output;
    double cycles = 0.0;
    double crack_length = input.initial_crack;
    while (true) {
        const double current_delta_k = delta_k(input, crack_length);
        const double current_max_k = max_k(input, crack_length);
        const double current_rate = growth_rate(material, input, crack_length);
        output.history.push_back({cycles, crack_length, current_delta_k, current_max_k, current_rate});
        if (current_max_k >= material.fracture_toughness || crack_length >= input.critical_crack) {
            output.termination = "fracture";
            break;
        }
        if (cycles >= input.max_cycles) {
            output.termination = "max_cycles";
            break;
        }
        const double step = std::min(input.cycles_per_step, input.max_cycles - cycles);
        crack_length += current_rate * step;
        cycles += step;
        if (crack_length >= input.critical_crack) {
            crack_length = input.critical_crack;
        }
    }
    return output;
}

inline nlohmann::json to_json(const SimulationOutput& output) {
    nlohmann::json result{{"termination", output.termination}, {"history", nlohmann::json::array()}};
    for (const auto& state : output.history) {
        result["history"].push_back({
            {"cycles", state.cycles},
            {"crack_length", state.crack_length},
            {"delta_k", state.delta_k},
            {"max_k", state.max_k},
            {"growth_rate", std::isfinite(state.growth_rate)
                                ? nlohmann::json(state.growth_rate)
                                : nlohmann::json(nullptr)}});
    }
    return result;
}

inline Material read_material_file(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("cannot open material file: " + path);
    }
    nlohmann::json json;
    file >> json;
    return material_from_json(json);
}

inline SimulationInput read_input_file(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("cannot open input file: " + path);
    }
    nlohmann::json json;
    file >> json;
    return input_from_json(json);
}

inline void write_output_file(const std::string& path, const SimulationOutput& output) {
    std::ofstream file(path);
    if (!file) {
        throw std::runtime_error("cannot open output file: " + path);
    }
    file << to_json(output).dump(2) << '\n';
}

} // namespace dta
