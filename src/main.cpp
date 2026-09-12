#include "dta/json.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

std::string read_input(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open input file: " + path.string());
    }
    std::string text;
    char buffer[8192];
    while (input.read(buffer, sizeof(buffer)) || input.gcount() > 0) {
        text.append(buffer, static_cast<std::size_t>(input.gcount()));
        if (text.size() > 4 * 1024 * 1024) {
            throw std::invalid_argument("input exceeds 4 MiB: " + path.string());
        }
    }
    if (input.bad()) {
        throw std::runtime_error("cannot read input file: " + path.string());
    }
    return text;
}

void protect_input(const std::filesystem::path& input, const std::filesystem::path& output) {
    if (std::filesystem::exists(output) && std::filesystem::equivalent(input, output)) {
        throw std::invalid_argument("input and output must be different files: " + input.string());
    }
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc != 3 && argc != 4) {
        std::cerr << "Usage: dta_assess INPUT.json MATERIAL.json OUTPUT.json\n"
                     "Legacy inline material: dta_assess INPUT.json OUTPUT.json\n";
        return 2;
    }
    try {
        const std::filesystem::path input_path(argv[1]);
        const std::filesystem::path output_path(argv[argc - 1]);
        const auto text = read_input(input_path);
        protect_input(input_path, output_path);
        std::string result;
        if (argc == 4) {
            const std::filesystem::path material_path(argv[2]);
            const auto material_text = read_input(material_path);
            protect_input(material_path, output_path);
            result = dta::assess_json(text, material_text);
        } else {
            result = dta::assess_json(text);
        }
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
