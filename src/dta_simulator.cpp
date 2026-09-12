#include <iostream>
#include <fstream>
#include <stdexcept>
#include <string>

#include "dta/crack_growth.hpp"
#include "dta/simulation.hpp"

int main(int argc, char* argv[]) {
    if (argc != 4 && argc != 5) {
        std::cerr << "usage: dta_simulator <input.json> <material.json> <output.json> [samples]\n";
        return 2;
    }
    try {
        const auto material = dta::read_material_file(argv[2]);
        const auto input = dta::read_input_file(argv[1]);
        if (argc == 4) {
            dta::write_output_file(argv[3], dta::assess_damage_tolerance(material, input));
        } else {
            dta::MonteCarloConfig config;
            config.samples = std::stoull(argv[4]);
            std::ofstream output(argv[3]);
            if (!output) throw std::runtime_error("cannot open output file: " + std::string(argv[3]));
            output << dta::simulation_to_json(
                dta::run_monte_carlo(material, input, config)).dump(2) << '\n';
        }
    } catch (const std::exception& error) {
        std::cerr << "dta_simulator: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
