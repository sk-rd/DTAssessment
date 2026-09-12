#pragma once

#include <cmath>
#include <fstream>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

#include "dta/spectrum.hpp"

namespace dta {

inline Spectrum read_spectrum_file(const std::string& path) {
    std::ifstream file(path);
    if (!file) throw std::runtime_error("cannot open spectrum file: " + path);
    nlohmann::json json;
    file >> json;
    Spectrum result{json.at("reference_stress"), {}};
    for (const auto& point : json.at("spectrum")) {
        if (!point.is_array() || point.size() != 2) {
            throw std::invalid_argument("spectrum must be an n x 2 time series");
        }
        result.points.push_back({point.at(0), point.at(1)});
    }
    if (result.reference_stress <= 0.0 || result.points.empty()) {
        throw std::invalid_argument("spectrum reference stress and points are required");
    }
    for (const auto& point : result.points) {
        if (point.time <= 0.0 || !std::isfinite(point.factor)) {
            throw std::invalid_argument("spectrum times must be positive and factors finite");
        }
    }
    return result;
}

} // namespace dta
