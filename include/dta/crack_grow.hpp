#pragma once

#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "dta/material.hpp"
#include "dta/ndi.hpp"
#include "dta/load.hpp"
#include "dta/spectrum.hpp"

namespace dta {

constexpr double pi = 3.14159265358979323846;

struct CrackGeometry { std::string type{"center_crack"}; double width{}; };
struct CrackState {
    double cycles{}, hours{}, crack_length{}, delta_k{}, max_k{}, growth_rate{};
};
struct SimulationInput {
    std::string material;
    std::string geometry{"center_crack"};
    double width{}, initial_crack{}, critical_crack{};
    double max_stress{}, min_stress{}, cycles_per_step{}, max_cycles{};
    NDI ndi;
    Spectrum spectrum;
};
struct SimulationOutput {
    std::string termination;
    std::vector<CrackState> history;
};

inline double geometry_factor(const CrackGeometry& geometry, double crack_length) {
    if (geometry.type == "edge_crack") return 1.12;
    return std::sqrt(1.0 / std::cos(pi * crack_length / geometry.width));
}
inline double delta_k(const CrackGeometry& geometry, const CrackLoad& load, double crack_length) {
    return geometry_factor(geometry, crack_length) * (load.max_stress - load.min_stress) *
           std::sqrt(pi * crack_length);
}
inline double max_k(const CrackGeometry& geometry, const CrackLoad& load, double crack_length) {
    return geometry_factor(geometry, crack_length) * load.max_stress *
           std::sqrt(pi * crack_length);
}
inline double growth_rate(const Material& material, const CrackGeometry& geometry,
                          const CrackLoad& load, double crack_length) {
    const double value = delta_k(geometry, load, crack_length);
    if (value <= material.threshold_delta_k) return 0.0;
    if (max_k(geometry, load, crack_length) >= material.fracture_toughness)
        return std::numeric_limits<double>::infinity();
    return material.c * std::pow(value, material.m);
}

class CrackGrow {
public:
    CrackGrow(const Material& material, const SimulationInput& input)
        : material_(material), input_(input) {}
    SimulationOutput run() const;
private:
    Material material_;
    SimulationInput input_;
};

} // namespace dta
