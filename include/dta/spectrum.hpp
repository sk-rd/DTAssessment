#pragma once

#include <string>
#include <vector>

namespace dta {

struct SpectrumPoint {
    double time{};
    double factor{};
};

struct Spectrum {
    double reference_stress{};
    std::vector<SpectrumPoint> points;
};

} // namespace dta
