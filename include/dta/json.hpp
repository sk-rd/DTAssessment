#pragma once

#include "dta/assessment.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace dta {

namespace json_detail {

using Json = nlohmann::json;

inline void keys(const Json& value, std::initializer_list<const char*> allowed) {
    if (!value.is_object()) {
        throw std::invalid_argument("expected a JSON object");
    }
    for (auto it = value.begin(); it != value.end(); ++it) {
        bool found = false;
        for (const auto* key : allowed) {
            found = found || it.key() == key;
        }
        if (!found) {
            throw std::invalid_argument("unknown field: " + it.key());
        }
    }
}

inline const Json& array(const Json& value) {
    if (!value.is_array()) {
        throw std::invalid_argument("expected a JSON array");
    }
    return value;
}

inline double number(const Json& value) {
    if (!value.is_number()) {
        throw std::invalid_argument("expected a finite number");
    }
    const double result = value.get<double>();
    if (!std::isfinite(result)) {
        throw std::invalid_argument("expected a finite number");
    }
    return result;
}

inline std::uint64_t integer(const Json& value, std::uint64_t maximum) {
    if (!value.is_number_integer() ||
        (!value.is_number_unsigned() && value.get<std::int64_t>() < 0)) {
        throw std::invalid_argument("expected a non-negative integer");
    }
    const auto result = value.get<std::uint64_t>();
    if (result > maximum) {
        throw std::invalid_argument("integer exceeds supported range");
    }
    return result;
}

inline unsigned count(const Json& value) {
    return static_cast<unsigned>(integer(value, std::numeric_limits<unsigned>::max()));
}

inline Json probability(unsigned events, unsigned trials) {
    const double n = trials;
    const double p = events / n;
    const double z = 1.959963984540054;
    const double denominator = 1 + z * z / n;
    const double center = (p + z * z / (2 * n)) / denominator;
    const double half = z * std::sqrt(p * (1 - p) / n + z * z / (4 * n * n)) / denominator;
    return {{"events", events}, {"probability", p},
            {"wilson_95", {std::max(0.0, center - half), std::min(1.0, center + half)}}};
}

inline Assessment assessment_from_json_impl(const Json& input, const Json& material) {
    keys(input, {"schema_version", "units", "simulation", "material", "spectrum", "fleet"});
    if (count(input.at("schema_version")) != 1) {
        throw std::invalid_argument("unsupported schema_version");
    }
    const auto& units = input.at("units");
    keys(units, {"length", "stress"});
    if (units.at("length") != "m" || units.at("stress") != "MPa") {
        throw std::invalid_argument("units must be length=m and stress=MPa");
    }
    Assessment result;
    const auto& simulation = input.at("simulation");
    keys(simulation, {"trials", "max_cycles", "history_interval", "seed"});
    result.trials = count(simulation.at("trials"));
    result.max_cycles = count(simulation.at("max_cycles"));
    result.history_interval = count(simulation.at("history_interval"));
    result.seed = integer(simulation.at("seed"), std::numeric_limits<std::uint64_t>::max());

    keys(material, {"paris_c", "paris_m", "toughness", "threshold", "walker_gamma"});
    result.material = {number(material.at("paris_c")), number(material.at("paris_m")),
                       number(material.at("toughness")), number(material.at("threshold")),
                       number(material.at("walker_gamma"))};
    for (const auto& block : array(input.at("spectrum"))) {
        keys(block, {"maximum", "minimum", "cycles"});
        result.spectrum.push_back(
            {number(block.at("maximum")), number(block.at("minimum")), count(block.at("cycles"))});
    }
    const auto& fleet = input.at("fleet");
    keys(fleet, {"id", "airplanes"});
    result.fleet_id = fleet.at("id").get<std::string>();
    for (const auto& airplane_json : array(fleet.at("airplanes"))) {
        keys(airplane_json, {"id", "major_components"});
        Airplane airplane;
        airplane.id = airplane_json.at("id").get<std::string>();
        for (const auto& major_json : array(airplane_json.at("major_components"))) {
            keys(major_json, {"id", "med_min_failed_components", "components"});
            MajorComponent major;
            major.id = major_json.at("id").get<std::string>();
            if (major_json.contains("med_min_failed_components")) {
                major.med_min_failed_components = count(major_json.at("med_min_failed_components"));
            }
            for (const auto& component_json : array(major_json.at("components"))) {
                keys(component_json, {"id", "msd_min_failed_details", "critical_details"});
                Component component;
                component.id = component_json.at("id").get<std::string>();
                if (component_json.contains("msd_min_failed_details")) {
                    component.msd_min_failed_details = count(component_json.at("msd_min_failed_details"));
                }
                for (const auto& detail_json : array(component_json.at("critical_details"))) {
                    keys(detail_json, {"id", "geometry", "width", "initial_crack"});
                    const auto& initial = detail_json.at("initial_crack");
                    keys(initial, {"distribution", "shape", "scale"});
                    if (initial.at("distribution") != "weibull") {
                        throw std::invalid_argument("initial_crack distribution must be weibull");
                    }
                    Detail detail;
                    detail.id = detail_json.at("id").get<std::string>();
                    detail.geometry = detail_json.at("geometry").get<std::string>();
                    detail.width = number(detail_json.at("width"));
                    detail.initial = {number(initial.at("shape")), number(initial.at("scale"))};
                    component.details.push_back(detail);
                }
                major.components.push_back(component);
            }
            airplane.major_components.push_back(major);
        }
        result.airplanes.push_back(airplane);
    }
    validate(result);
    return result;
}

inline Json parse_json(const std::string& text) {
    if (text.size() > 4 * 1024 * 1024) {
        throw std::invalid_argument("input exceeds 4 MiB");
    }
    std::vector<std::set<std::string>> object_keys;
    const auto callback = [&object_keys](int depth, nlohmann::json::parse_event_t event,
                                        nlohmann::json& value) {
        if (depth > 32) {
            throw std::invalid_argument("JSON nesting exceeds 32 levels");
        }
        using Event = nlohmann::json::parse_event_t;
        if (event == Event::object_start) {
            object_keys.emplace_back();
        } else if (event == Event::key) {
            if (!object_keys.back().insert(value.get<std::string>()).second) {
                throw std::invalid_argument("duplicate JSON field: " + value.get<std::string>());
            }
        } else if (event == Event::object_end) {
            object_keys.pop_back();
        }
        return true;
    };
    return Json::parse(text, callback);
}

} // namespace json_detail

inline Assessment assessment_from_json(const nlohmann::json& input) {
    return json_detail::assessment_from_json_impl(input, input.at("material"));
}

inline Assessment assessment_from_json(const nlohmann::json& input,
                                       const nlohmann::json& material) {
    if (input.contains("material")) {
        throw std::invalid_argument("inline material is not allowed with a separate material input");
    }
    return json_detail::assessment_from_json_impl(input, material);
}

inline Assessment parse_assessment(const std::string& text) {
    return assessment_from_json(json_detail::parse_json(text));
}

inline Assessment parse_assessment(const std::string& text, const std::string& material_text) {
    const auto input = json_detail::parse_json(text);
    const auto material = json_detail::parse_json(material_text);
    return assessment_from_json(input, material);
}

inline nlohmann::json result_to_json(const Assessment& input, const Result& result) {
    using namespace json_detail;
    if (result.trials == 0 || result.trials != input.trials) {
        throw std::invalid_argument("result trial count must match input");
    }
    Json details = Json::array();
    for (const auto& detail : result.representative_details) {
        Json history = Json::array();
        for (const auto& point : detail.history) {
            history.push_back({{"cycle", point.cycle}, {"crack_size", point.crack_size},
                               {"residual_strength", point.residual_strength}});
        }
        details.push_back({{"path", detail.path}, {"initial_size", detail.initial_size},
                           {"final_size", detail.final_size}, {"failed", detail.failed},
                           {"life_cycles", detail.life_cycles}, {"censored", !detail.failed},
                           {"failure_mode", detail.failure_mode}, {"history", history}});
    }
    Json risks = Json::array();
    for (const auto& risk : result.risks) {
        if (risk.failed_trials > result.trials || risk.msd_trials > result.trials ||
            risk.med_trials > result.trials) {
            throw std::invalid_argument("event count exceeds trial count");
        }
        risks.push_back({{"path", risk.path}, {"level", risk.level},
                         {"failure", probability(risk.failed_trials, result.trials)},
                         {"msd", probability(risk.msd_trials, result.trials)},
                         {"med", probability(risk.med_trials, result.trials)}});
    }
    return {{"schema_version", 1},
            {"model", "Paris-Walker independent-crack screening prototype"},
            {"limitations", {
                "Not validated for airworthiness or certification.",
                "MSD/MED are failed-site co-occurrence screens, not crack interaction or load redistribution.",
                "Parent failure is the conservative union of child failures.",
                "Geometry validity-limit events are treated conservatively as failures.",
                "No overload retardation, crack closure history, initiation, corrosion or inspection model.",
                "Curves and censored life_cycles describe trial 0 only; probabilities describe the full ensemble.",
                "Wilson intervals quantify Monte Carlo sampling uncertainty only."}},
            {"units", {{"length", "m"}, {"stress", "MPa"}, {"stress_intensity", "MPa*sqrt(m)"},
                       {"life", "cycles"}}},
            {"simulation", {{"trials", result.trials}, {"seed", input.seed},
                            {"max_cycles", input.max_cycles}, {"history_interval", input.history_interval}}},
            {"representative_trial", 0}, {"critical_details", details}, {"reliability", risks}};
}

inline std::string assess_json(const std::string& text) {
    const auto input = parse_assessment(text);
    return result_to_json(input, assess(input)).dump(2);
}

inline std::string assess_json(const std::string& text, const std::string& material_text) {
    const auto input = parse_assessment(text, material_text);
    return result_to_json(input, assess(input)).dump(2);
}

} // namespace dta
