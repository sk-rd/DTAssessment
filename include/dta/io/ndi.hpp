#pragma once

#include <fstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "dta/ndi.hpp"

namespace dta {

inline NDI read_ndi_file(const std::string& path) {
    std::ifstream file(path);
    if (!file) throw std::runtime_error("cannot open NDI file: " + path);
    nlohmann::json json;
    file >> json;
    NDI result{json.at("name_zh"), json.at("name_en"), json.at("start_cycles"),
               json.at("interval_cycles"), {}};
    for (const auto& point : json.at("pod")) {
        result.pod.push_back({point.at("crack_length"), point.at("probability")});
    }
    if (result.interval_cycles <= 0.0 || result.pod.empty()) {
        throw std::invalid_argument("NDI interval and POD curve are required");
    }
    return result;
}

} // namespace dta
