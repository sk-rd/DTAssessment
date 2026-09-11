#include <boost/program_options.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace po = boost::program_options;

constexpr double pi = 3.14159265358979323846;

struct Crack {
    std::size_t id{};
    double x{};
    double y{};
    double length{};
    double angle{};
    bool active{true};
};

struct SimulationParameters {
    double width{100.0};
    double height{100.0};
    double max_cycles{1000.0};
    double stress_min{10.0};
    double stress_max{100.0};
    double geometry_factor{1.12};
    double threshold{5.0};
    double paris_c{1.0e-10};
    double paris_m{3.0};
    double interaction_radius{20.0};
    double merge_distance{1.0};
    double initial_length{1.0};
    std::size_t crack_count{5};
    std::uint32_t seed{42};
    std::string output{"crack_history.csv"};
};

double distance(const Crack& lhs, const Crack& rhs) {
    return std::hypot(lhs.x - rhs.x, lhs.y - rhs.y);
}

double interaction_factor(const Crack& crack, const std::vector<Crack>& cracks,
                          const SimulationParameters& parameters) {
    double factor = 1.0;
    for (const auto& other : cracks) {
        if (!other.active || other.id == crack.id) {
            continue;
        }

        const double separation = distance(crack, other);
        if (separation < parameters.interaction_radius) {
            factor += 0.15 * (1.0 - separation / parameters.interaction_radius);
        }
    }
    return factor;
}

void merge_cracks(std::vector<Crack>& cracks, double merge_distance) {
    for (std::size_t i = 0; i < cracks.size(); ++i) {
        if (!cracks[i].active) {
            continue;
        }
        for (std::size_t j = i + 1; j < cracks.size(); ++j) {
            if (!cracks[j].active || distance(cracks[i], cracks[j]) > merge_distance) {
                continue;
            }

            const double combined_length = cracks[i].length + cracks[j].length;
            cracks[i].angle = std::atan2(
                cracks[i].length * std::sin(cracks[i].angle) +
                    cracks[j].length * std::sin(cracks[j].angle),
                cracks[i].length * std::cos(cracks[i].angle) +
                    cracks[j].length * std::cos(cracks[j].angle));
            cracks[i].length = combined_length;
            cracks[j].active = false;
        }
    }
}

std::vector<Crack> create_initial_cracks(const SimulationParameters& parameters) {
    std::mt19937 generator(parameters.seed);
    std::uniform_real_distribution<double> x_distribution(0.05 * parameters.width,
                                                            0.95 * parameters.width);
    std::uniform_real_distribution<double> y_distribution(0.05 * parameters.height,
                                                            0.95 * parameters.height);
    std::uniform_real_distribution<double> angle_distribution(0.0, 2.0 * pi);

    std::vector<Crack> cracks;
    cracks.reserve(parameters.crack_count);
    for (std::size_t i = 0; i < parameters.crack_count; ++i) {
        cracks.push_back({i, x_distribution(generator), y_distribution(generator),
                          parameters.initial_length, angle_distribution(generator), true});
    }
    return cracks;
}

void write_header(std::ofstream& output) {
    output << "cycle,crack_id,x,y,length,angle,active,delta_k,interaction_factor\n";
}

void write_state(std::ofstream& output, std::size_t cycle, const Crack& crack,
                 double delta_k, double interaction) {
    output << cycle << ',' << crack.id << ',' << std::setprecision(10) << crack.x << ','
           << crack.y << ',' << crack.length << ',' << crack.angle << ','
           << (crack.active ? 1 : 0) << ',' << delta_k << ',' << interaction << '\n';
}

void run_simulation(const SimulationParameters& parameters) {
    if (parameters.stress_max <= parameters.stress_min || parameters.crack_count == 0 ||
        parameters.initial_length <= 0.0 || parameters.max_cycles <= 0.0) {
        throw std::invalid_argument("invalid simulation parameters");
    }

    std::ofstream output(parameters.output);
    if (!output) {
        throw std::runtime_error("cannot open output file: " + parameters.output);
    }
    write_header(output);

    auto cracks = create_initial_cracks(parameters);
    std::size_t cycle = 0;
    for (; cycle < static_cast<std::size_t>(parameters.max_cycles); ++cycle) {
        bool any_active = false;
        for (auto& crack : cracks) {
            if (!crack.active) {
                write_state(output, cycle, crack, 0.0, 1.0);
                continue;
            }

            any_active = true;
            const double interaction = interaction_factor(crack, cracks, parameters);
            const double delta_stress = parameters.stress_max - parameters.stress_min;
            const double delta_k = parameters.geometry_factor * interaction * delta_stress *
                                   std::sqrt(pi * crack.length);
            double growth = 0.0;
            if (delta_k > parameters.threshold) {
                growth = parameters.paris_c * std::pow(delta_k, parameters.paris_m);
                crack.length += growth;
                crack.x = std::clamp(crack.x + growth * std::cos(crack.angle), 0.0,
                                     parameters.width);
                crack.y = std::clamp(crack.y + growth * std::sin(crack.angle), 0.0,
                                     parameters.height);
            }
            write_state(output, cycle, crack, delta_k, interaction);
        }

        merge_cracks(cracks, parameters.merge_distance);
        if (!any_active) {
            break;
        }
    }

    std::size_t active_count = std::count_if(
        cracks.begin(), cracks.end(), [](const Crack& crack) { return crack.active; });
    std::cout << "Simulation completed after " << cycle << " cycles; " << active_count
              << " active cracks remain. Results: " << parameters.output << '\n';
}

int main(int argc, char* argv[]) {
    SimulationParameters parameters;
    po::options_description options("DTAssessment multi-crack simulation options");
    options.add_options()
        ("help,h", "show this help message")
        ("cracks,n", po::value<std::size_t>(&parameters.crack_count)->default_value(parameters.crack_count),
         "number of initial cracks")
        ("cycles", po::value<double>(&parameters.max_cycles)->default_value(parameters.max_cycles),
         "maximum number of load cycles")
        ("stress-min", po::value<double>(&parameters.stress_min)->default_value(parameters.stress_min),
         "minimum applied stress")
        ("stress-max", po::value<double>(&parameters.stress_max)->default_value(parameters.stress_max),
         "maximum applied stress")
        ("threshold", po::value<double>(&parameters.threshold)->default_value(parameters.threshold),
         "crack-growth threshold for delta K")
        ("seed", po::value<std::uint32_t>(&parameters.seed)->default_value(parameters.seed),
         "random seed")
        ("output,o", po::value<std::string>(&parameters.output)->default_value(parameters.output),
         "CSV output path");

    try {
        po::variables_map variables;
        po::store(po::parse_command_line(argc, argv, options), variables);
        po::notify(variables);
        if (variables.count("help") != 0U) {
            std::cout << options << '\n';
            return 0;
        }
        run_simulation(parameters);
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        std::cerr << options << '\n';
        return 1;
    }
    return 0;
}
