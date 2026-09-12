#include <iostream>
#include <fstream>
#include <stdexcept>
#include <string>

#include "dta/src.hpp"

int main(int argc, char* argv[]) {
    if (argc != 5 && argc != 6) {
        std::cerr << "usage: dta_simulator <input.json> <material.json> <ndi.json> <output.json> [samples]\n";
        return 2;
    }
    try {
        const auto material = dta::read_material_file(argv[2]);
        const auto input = dta::read_input_file(argv[1]);
        const auto ndi = dta::read_ndi_file(argv[3]);
        if (argc == 5) {
            dta::write_output_file(argv[4], dta::assess_damage_tolerance(material, input));
        } else {
            dta::SimulationConfig config;
            config.samples = std::stoull(argv[5]);
            dta::write_simulation_file(argv[4],
                dta::Simulation(material, input, config, ndi).run());
        }
    } catch (const std::exception& error) {
        std::cerr << "dta_simulator: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
