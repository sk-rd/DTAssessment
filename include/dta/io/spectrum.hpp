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
        result.points.push_back({point.at("cycles"), point.at("hours"), point.at("factor")});
    }
    if (result.reference_stress <= 0.0 || result.points.empty()) {
        throw std::invalid_argument("spectrum reference stress and points are required");
    }
    for (const auto& point : result.points) {
        if (point.cycles <= 0.0 || point.hours <= 0.0 || !std::isfinite(point.factor)) {
            throw std::invalid_argument("spectrum cycles/hours must be positive and factors finite");
        }
    }
    return result;
}

} // namespace dta
