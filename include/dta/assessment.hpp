#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace dta {

// Research/screening prototype, not certification software. Use metres, MPa,
// cycles and MPa*sqrt(m); Paris C must use the corresponding units.
struct Weibull { double shape = 2, scale = 0.001; };
struct Material {
    double paris_c = 1e-10, paris_m = 3, toughness = 40, threshold = 0,
           walker_gamma = 1;
};
struct LoadBlock { double maximum = 100, minimum = 0; unsigned cycles = 1; };
struct Detail {
    std::string id;
    std::string geometry = "center_crack";
    double width = 0.1;
    Weibull initial;
};
struct Component {
    std::string id;
    std::vector<Detail> details;
    unsigned msd_min_failed_details = 2;
};
struct MajorComponent {
    std::string id;
    std::vector<Component> components;
    unsigned med_min_failed_components = 2;
};
struct Airplane {
    std::string id;
    std::vector<MajorComponent> major_components;
};
struct Assessment {
    std::string fleet_id;
    Material material;
    std::vector<LoadBlock> spectrum;
    std::vector<Airplane> airplanes;
    unsigned trials = 100, max_cycles = 10000, history_interval = 100;
    std::uint64_t seed = 42;
};
struct Point { unsigned cycle; double crack_size, residual_strength; };
struct DetailResult {
    std::string path;
    double initial_size, final_size;
    bool failed;
    // !failed means right-censored at max_cycles, never an observed failure.
    unsigned life_cycles;
    std::string failure_mode;
    std::vector<Point> history;
};
struct Risk {
    std::string path, level;
    unsigned failed_trials = 0, msd_trials = 0, med_trials = 0;
};
struct Result {
    unsigned trials;
    std::vector<DetailResult> representative_details;
    std::vector<Risk> risks;
};

namespace detail {
constexpr double pi = 3.14159265358979323846;
constexpr std::uint64_t max_nodes = 10000;
constexpr std::uint64_t max_detail_cycles = 20000000;
constexpr std::uint64_t max_history_points = 200000;
constexpr std::uint64_t max_substeps = 50000000;

inline void require(bool condition, const char* message) {
    if (!condition) throw std::invalid_argument(message);
}
inline bool positive(double value) {
    return std::isfinite(value) && value > 0;
}
inline bool nonnegative(double value) {
    return std::isfinite(value) && value >= 0;
}
inline void check_id(const std::string& id) {
    require(!id.empty() && id.size() <= 128,
            "IDs must contain 1..128 ASCII letters, digits, '_' or '-'");
    for (unsigned char c : id)
        require((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') || c == '_' || c == '-',
                "IDs must contain 1..128 ASCII letters, digits, '_' or '-'");
}
template<class T>
inline void check_siblings(const std::vector<T>& children) {
    require(!children.empty(), "Every hierarchy node must have children");
    require(children.size() <= max_nodes, "Hierarchy exceeds 10000 nodes");
    std::set<std::string> ids;
    for (const auto& child : children) {
        check_id(child.id);
        require(ids.insert(child.id).second, "Duplicate sibling ID");
    }
}
inline double domain_ratio(const std::string& geometry) {
    if (geometry == "center_crack") return 0.5;
    if (geometry == "edge_crack") return 0.6;
    throw std::invalid_argument("Unknown geometry: " + geometry);
}
} // namespace detail

// Analytical SIF database for uniform remote tension, full plate width W.
// center_crack: a is half-length, Y = sqrt(sec(pi*a/W)), 0 < a/W < 0.5.
// edge_crack: a is edge depth, standard finite-width polynomial,
// 0 < a/W < 0.6. This fit must NOT be extrapolated to ligament collapse.
inline double geometry_factor(const std::string& geometry,
                              double crack_size, double width) {
    detail::require(detail::positive(width) && detail::positive(crack_size),
                    "Geometry requires finite positive crack size and width");
    const double ratio = crack_size / width;
    detail::require(ratio < detail::domain_ratio(geometry),
                    "Crack is outside the geometry validity domain");
    if (geometry == "center_crack")
        return 1 / std::sqrt(std::cos(detail::pi * ratio));
    return 1.12 - 0.231 * ratio + 10.55 * ratio * ratio
           - 21.72 * ratio * ratio * ratio
           + 30.39 * ratio * ratio * ratio * ratio;
}

// Explicit limits bound CPU and first-trial output memory. Runtime additionally
// caps adaptive integration at 4096 substeps/cycle and 50 million overall.
// Exhaustion/numerical overflow throws; it is NOT counted as physical failure.
inline void validate(const Assessment& input) {
    detail::check_id(input.fleet_id);
    detail::require(input.trials > 0 && input.trials <= 10000,
                    "trials must be in [1,10000]");
    detail::require(input.max_cycles > 0 && input.max_cycles <= 1000000,
                    "max_cycles must be in [1,1000000]");
    detail::require(input.history_interval > 0, "history_interval must be positive");
    const auto& m = input.material;
    detail::require(detail::nonnegative(m.paris_c) && detail::positive(m.paris_m) &&
                    detail::positive(m.toughness) && detail::nonnegative(m.threshold) &&
                    detail::nonnegative(m.walker_gamma) && m.walker_gamma <= 1,
                    "Invalid material: C>=0, m>0, Kc>0, threshold>=0, 0<=gamma<=1; all finite");
    detail::require(!input.spectrum.empty() && input.spectrum.size() <= 10000,
                    "Spectrum must contain 1..10000 blocks");
    for (const auto& block : input.spectrum)
        detail::require(detail::positive(block.maximum) &&
                        std::isfinite(block.minimum) && block.minimum < block.maximum &&
                        block.cycles > 0, "Load blocks require finite max>0, min<max, cycles>0");
    detail::check_siblings(input.airplanes);
    std::uint64_t nodes = 1, details = 0;
    auto count_node = [&]() {
        detail::require(++nodes <= detail::max_nodes, "Hierarchy exceeds 10000 nodes");
    };
    for (const auto& airplane : input.airplanes) {
        count_node();
        detail::check_siblings(airplane.major_components);
        for (const auto& major : airplane.major_components) {
            count_node();
            detail::require(major.med_min_failed_components >= 2, "MED threshold must be >=2");
            detail::check_siblings(major.components);
            for (const auto& component : major.components) {
                count_node();
                detail::require(component.msd_min_failed_details >= 2, "MSD threshold must be >=2");
                detail::check_siblings(component.details);
                for (const auto& site : component.details) {
                    count_node();
                    ++details;
                    detail::domain_ratio(site.geometry);
                    detail::require(detail::positive(site.width) &&
                                    detail::positive(site.width * detail::domain_ratio(site.geometry)) &&
                                    detail::positive(site.initial.shape) &&
                                    detail::positive(site.initial.scale),
                                    "Width and Weibull shape/scale must be finite and positive");
                }
            }
        }
    }
    detail::require(details * input.trials * input.max_cycles <= detail::max_detail_cycles,
                    "Work limit: details * trials * max_cycles must be <=20000000");
    const std::uint64_t points = 1 + input.max_cycles / input.history_interval +
                                (input.max_cycles % input.history_interval != 0);
    detail::require(details * points <= detail::max_history_points,
                    "History limit: first-trial points must be <=200000");
}

namespace detail {
inline double residual_strength(const Detail& site, const Material& m, double a) {
    // Zero is a conservative sentinel outside the SIF domain, not a computed
    // post-collapse strength. No yield strength/net-section model is supplied.
    if (a >= site.width * domain_ratio(site.geometry)) return 0;
    const double value = m.toughness / geometry_factor(site.geometry, a, site.width)
                         / std::sqrt(a) / std::sqrt(pi);
    if (!std::isfinite(value))
        throw std::runtime_error("Residual strength exceeds numerical range");
    return value;
}
inline std::string failure(const Detail& site, const Material& m,
                           double a, double maximum) {
    const double ligament_limit = site.width * (site.geometry == "center_crack" ? 0.5 : 1.0);
    if (a >= ligament_limit) return "ligament";
    if (a >= site.width * domain_ratio(site.geometry)) return "geometry_domain";
    if (maximum >= residual_strength(site, m, a)) return "toughness";
    return {};
}
inline DetailResult simulate(const Detail& site, const std::string& path,
                             const Assessment& input, std::mt19937_64& rng,
                             bool record, std::uint64_t& total_substeps) {
    std::weibull_distribution<double> distribution(site.initial.shape, site.initial.scale);
    const double initial = distribution(rng);
    if (!positive(initial))
        throw std::runtime_error("Weibull sample exceeds numerical range; revise shape/scale");
    DetailResult result{path, initial, initial, false, input.max_cycles, "runout", {}};
    auto save = [&](unsigned cycle) {
        if (!record) return;
        Point p{cycle, result.final_size, residual_strength(site, input.material, result.final_size)};
        if (!result.history.empty() && result.history.back().cycle == cycle)
            result.history.back() = p;
        else
            result.history.push_back(p);
    };
    auto check_failure = [&](unsigned cycle, double maximum) {
        auto mode = failure(site, input.material, result.final_size, maximum);
        if (mode.empty()) return false;
        result.failed = true;
        result.life_cycles = cycle;
        result.failure_mode = mode;
        return true;
    };
    save(0);
    if (check_failure(0, input.spectrum.front().maximum)) return result;
    std::size_t block_index = 0;
    unsigned block_cycle = 0;
    for (unsigned cycle = 1; cycle <= input.max_cycles; ++cycle) {
        const auto& block = input.spectrum[block_index];
        if (!check_failure(cycle, block.maximum)) {
            double remaining = 1;
            unsigned substeps = 0;
            while (remaining > 0) {
                if (++substeps > 4096 || ++total_substeps > max_substeps)
                    throw std::runtime_error("Adaptive work limit: 4096 substeps/cycle or 50000000 total");
                const auto& m = input.material;
                const double a = result.final_size;
                // Walker: ΔK_eff = Kmax*(1-R_open)^gamma. Compression is
                // closed-crack loading, R_open=max(0,min/max). No retardation,
                // closure memory, coalescence or load redistribution is modeled.
                const double opening = (block.maximum - std::max(0.0, block.minimum)) / block.maximum;
                const double effective = block.maximum * geometry_factor(site.geometry, a, site.width)
                                         * std::sqrt(a) * std::sqrt(pi)
                                         * std::pow(opening, m.walker_gamma);
                if (!std::isfinite(effective))
                    throw std::runtime_error("Stress intensity exceeds numerical range");
                if (effective <= m.threshold || m.paris_c == 0) break;
                const double rate = m.paris_c * std::pow(effective, m.paris_m);
                if (!std::isfinite(rate))
                    throw std::runtime_error("Paris growth exceeds numerical range");
                if (rate == 0) break;
                const double limit = site.width * domain_ratio(site.geometry);
                // Forward Euler with at most 2% fractional crack growth.
                const double full_increment = rate * remaining;
                const double increment = std::min({full_increment, 0.02 * a, limit - a});
                // An unrepresentably small increment has no resolvable effect.
                if (increment <= 0 || a + increment == a) break;
                result.final_size = std::min(limit, a + increment);
                remaining = increment == full_increment ? 0 :
                            std::max(0.0, remaining - increment / rate);
                if (check_failure(cycle, block.maximum)) break;
            }
        }
        if (result.failed) {
            save(cycle);
            return result;
        }
        if (cycle % input.history_interval == 0) save(cycle);
        if (++block_cycle == block.cycles) {
            block_cycle = 0;
            block_index = (block_index + 1) % input.spectrum.size();
        }
    }
    save(input.max_cycles);
    return result;
}
struct Event { bool failed = false, msd = false, med = false; };
inline void merge(Event& parent, const Event& child) {
    parent.failed = parent.failed || child.failed;
    parent.msd = parent.msd || child.msd;
    parent.med = parent.med || child.med;
}
} // namespace detail

// Independent Weibull initial cracks; deterministic input ordering and fixed
// seed are reproducible within a standard-library implementation. Spectrum
// blocks repeat to max_cycles. LFD is the union of failed critical details;
// MSD/MED are co-occurrence screens by the horizon, not physical interaction.
inline Result assess(const Assessment& input) {
    validate(input);
    Result result{input.trials, {}, {}};
    std::mt19937_64 rng(input.seed);
    std::uint64_t substeps = 0;
    for (unsigned trial = 0; trial < input.trials; ++trial) {
        std::size_t next_risk = 0;
        auto node = [&](const std::string& path, const std::string& level) {
            const auto index = next_risk++;
            if (trial == 0) result.risks.push_back(Risk{path, level});
            return index;
        };
        auto tally = [&](std::size_t index, const detail::Event& event) {
            auto& risk = result.risks[index];
            risk.failed_trials += event.failed;
            risk.msd_trials += event.msd;
            risk.med_trials += event.med;
        };
        const auto fleet_index = node(input.fleet_id, "fleet");
        detail::Event fleet;
        for (const auto& airplane : input.airplanes) {
            const auto ap = input.fleet_id + "/" + airplane.id;
            const auto ai = node(ap, "airplane");
            detail::Event ae;
            for (const auto& major : airplane.major_components) {
                const auto mp = ap + "/" + major.id;
                const auto mi = node(mp, "major_component");
                detail::Event me;
                unsigned failed_components = 0;
                for (const auto& component : major.components) {
                    const auto cp = mp + "/" + component.id;
                    const auto ci = node(cp, "component");
                    detail::Event ce;
                    unsigned failed_details = 0;
                    for (const auto& site : component.details) {
                        const auto dp = cp + "/" + site.id;
                        const auto di = node(dp, "critical_detail");
                        auto dr = detail::simulate(site, dp, input, rng, trial == 0, substeps);
                        detail::Event de{dr.failed, false, false};
                        tally(di, de);
                        failed_details += dr.failed;
                        detail::merge(ce, de);
                        if (trial == 0) result.representative_details.push_back(std::move(dr));
                    }
                    ce.msd = failed_details >= component.msd_min_failed_details;
                    tally(ci, ce);
                    failed_components += ce.failed;
                    detail::merge(me, ce);
                }
                me.med = failed_components >= major.med_min_failed_components;
                tally(mi, me);
                detail::merge(ae, me);
            }
            tally(ai, ae);
            detail::merge(fleet, ae);
        }
        tally(fleet_index, fleet);
    }
    return result;
}

} // namespace dta
