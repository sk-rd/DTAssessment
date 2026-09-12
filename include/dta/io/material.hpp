#pragma once

#include <fstream>
#include <stdexcept>
#include <filesystem>
#include <string>

#include <nlohmann/json.hpp>

#include "dta/material.hpp"

namespace dta {

inline Material read_material_file(const std::string& path) {
    std::ifstream file(path);
    if (!file) throw std::runtime_error("cannot open material file: " + path);
    nlohmann::json json;
    file >> json;
    return validate_material(Material{json.at("c"), json.at("m"),
                                      json.at("threshold_delta_k"),
                                      json.at("fracture_toughness")});
}

inline Material read_material_database(const std::string& input_path,
                                       const std::string& material_name) {
    const auto database = std::filesystem::path(input_path).parent_path() /
                          "material" / (material_name + ".json");
    return read_material_file(database.string());
}

} // namespace dta
