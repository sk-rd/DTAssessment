#pragma once

#include "dta/damage_tolerance.hpp"

namespace dta {

class Sample {
public:
    Sample(const Material& material, const SimulationInput& input)
        : material_(material), input_(input) {}

    SimulationOutput run() const {
        return assess_damage_tolerance(material_, input_);
    }

private:
    Material material_;
    SimulationInput input_;
};

} // namespace dta
