#pragma once

#include <string>
#include <vector>

namespace dta {

struct SpectrumPoint {
    double cycles{};
    double hours{};
    double factor{};
};

struct Spectrum {
    double reference_stress{};
    std::vector<SpectrumPoint> points;
};

} // namespace dta
