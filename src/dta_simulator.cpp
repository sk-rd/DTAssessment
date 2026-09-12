#include <iostream>
#include <fstream>
#include <stdexcept>
#include <string>

#include "dta/src.hpp"

int main(int argc, char* argv[]) {
    if (argc != 3 && argc != 4) {
        std::cerr << "usage: dta_simulator <input.json> <output.json> [samples]\n";
        return 2;
    }
    try {
        const auto input = dta::read_input_file(argv[1]);
        const auto material = dta::read_material_database(argv[1], input.material);
        if (argc == 3) {
            dta::write_output_file(argv[2], dta::assess_damage_tolerance(material, input));
        } else {
            dta::SimulationConfig config;
            config.samples = std::stoull(argv[3]);
            dta::write_simulation_file(argv[2],
                dta::Simulation(material, input, config).run());
        }
    } catch (const std::exception& error) {
        std::cerr << "dta_simulator: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
