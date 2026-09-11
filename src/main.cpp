#include "dta/json.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: dta_assess INPUT.json OUTPUT.json\n";
        return 2;
    }
    try {
        const std::filesystem::path input_path(argv[1]);
        const std::filesystem::path output_path(argv[2]);
        if (std::filesystem::exists(output_path) &&
            std::filesystem::equivalent(input_path, output_path)) {
            throw std::invalid_argument("input and output must be different files");
        }
        std::ifstream input(input_path, std::ios::binary);
        if (!input) {
            throw std::runtime_error("cannot open input file");
        }
        std::string text;
        char buffer[8192];
        while (input.read(buffer, sizeof(buffer)) || input.gcount() > 0) {
            text.append(buffer, static_cast<std::size_t>(input.gcount()));
            if (text.size() > 4 * 1024 * 1024) {
                throw std::invalid_argument("input exceeds 4 MiB");
            }
        }
        if (input.bad()) {
            throw std::runtime_error("cannot read input file");
        }
        const auto result = dta::assess_json(text);
        std::ofstream output(output_path, std::ios::binary);
        if (!output) {
            throw std::runtime_error("cannot open output file");
        }
        output << result << '\n';
        output.close();
        if (!output) {
            throw std::runtime_error("cannot write output file");
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << nlohmann::json({{"error", error.what()}}).dump() << '\n';
        return 1;
    }
}
