#pragma once

#include "dta/crack_grow.hpp"

namespace dta {

class Sample {
public:
    Sample(const Material& material, const SimulationInput& input)
        : material_(material), input_(input) {}

    SimulationOutput run() const {
        return CrackGrow(material_, input_).run();
    }

private:
    Material material_;
    SimulationInput input_;
};

} // namespace dta
