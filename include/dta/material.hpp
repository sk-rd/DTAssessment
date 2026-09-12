#pragma once

#include <cmath>
#include <string>

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

inline Material validate_material(Material material) {
    require_positive("c", material.c);
    require_positive("m", material.m);
    require_positive("threshold_delta_k", material.threshold_delta_k);
    require_positive("fracture_toughness", material.fracture_toughness);
    return material;
}

} // namespace dta
