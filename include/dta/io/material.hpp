#pragma once

#include <fstream>
#include <stdexcept>
#include <string>

#include <nlohmann/json.hpp>

#include "dta/material.hpp"

namespace dta {

inline Material read_material_file(const std::string& path) {
    return material_from_json(path);
}

} // namespace dta
