#include <iostream>
#include <stdexcept>
#include <string>

#include "dta/crack_growth.hpp"

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr << "usage: dta_simulator <input.json> <material.json> <output.json>\n";
        return 2;
    }
    try {
        const auto material = dta::read_material_file(argv[2]);
        const auto input = dta::read_input_file(argv[1]);
        dta::write_output_file(argv[3], dta::simulate(material, input));
    } catch (const std::exception& error) {
        std::cerr << "dta_simulator: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
