#pragma once

#include <fstream>
#include <stdexcept>
#include <filesystem>

#include <nlohmann/json.hpp>

#include "dta/ndi.hpp"

namespace dta {

inline NDI read_ndi_file(const std::string& path) {
    std::ifstream file(path);
    if (!file) throw std::runtime_error("cannot open NDI file: " + path);
    nlohmann::json json;
    file >> json;
    NDI result{json.at("name_zh"), json.at("name_en"), 0.0, 0.0, 0.0, 0.0, {}};
    for (const auto& point : json.at("pod")) {
        result.pod.push_back({point.at("crack_length"), point.at("probability")});
    }

    if (result.pod.empty()) {
        throw std::invalid_argument("NDI POD curve is required");
    }
    return result;
}

inline NDI read_ndi_database(const std::string& input_path, const NDI& plan) {
    const auto database = std::filesystem::path(input_path).parent_path() /
                          "NDI" / (plan.name_zh + ".json");
    auto database_ndi = read_ndi_file(database.string());
    database_ndi.threshold_cycles = plan.threshold_cycles;
    database_ndi.threshold_hours = plan.threshold_hours;
    database_ndi.interval_cycles = plan.interval_cycles;
    database_ndi.interval_hours = plan.interval_hours;
    return database_ndi;
}

} // namespace dta
