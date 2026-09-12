#pragma once

#include <cmath>
#include <fstream>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

namespace dta {

struct Material {
    double c{};
    double m{};
    double threshold_delta_k{};
    double fracture_toughness{};
};

inline void require_positive(const char* name, double value) {
    if (!(value > 0.0) || !std::isfinite(value)) {
        throw std::invalid_argument(std::string(name) + " must be positive and finite");
    }
}

inline Material material_from_json(const std::string& filename) {
    std::ifstream file(filename);
    if (!file) throw std::runtime_error("cannot open material file: " + filename);
    nlohmann::json json;
    file >> json;
    Material material{json.at("c"), json.at("m"), json.at("threshold_delta_k"),
                      json.at("fracture_toughness")};
    require_positive("c", material.c);
    require_positive("m", material.m);
    require_positive("threshold_delta_k", material.threshold_delta_k);
    require_positive("fracture_toughness", material.fracture_toughness);
    return material;
}

} // namespace dta
