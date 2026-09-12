#include <boost/test/unit_test.hpp>

#include "dta/json.hpp"

#include <fstream>
#include <limits>
#include <sstream>
#include <string>

namespace {

nlohmann::json example() {
    std::ifstream file(std::string(DTA_SOURCE_DIR) + "/data/example.json");
    nlohmann::json input;
    file >> input;
    std::ifstream material(std::string(DTA_SOURCE_DIR) + "/data/material.json");
    material >> input["material"];
    input["simulation"]["trials"] = 3;
    input["simulation"]["max_cycles"] = 10;
    return input;
}

} // namespace

BOOST_AUTO_TEST_CASE(json_example_produces_typed_output) {
    const auto input = example();
    const auto output = nlohmann::json::parse(dta::assess_json(input.dump()));
    BOOST_TEST(output.at("schema_version").get<int>() == 1);
    BOOST_TEST(output.at("critical_details").size() == 4u);
    BOOST_TEST(output.at("reliability").size() == 9u);
    BOOST_TEST(output.at("simulation").at("trials").get<unsigned>() == 3u);
    for (const auto& detail : output.at("critical_details")) {
        BOOST_TEST(detail.at("failed").is_boolean());
        BOOST_TEST(detail.at("history").is_array());
        BOOST_TEST(detail.at("history").front().at("cycle").get<unsigned>() == 0u);
        BOOST_TEST(detail.at("history").back().at("cycle") == detail.at("life_cycles"));
        BOOST_TEST(detail.at("censored").get<bool>() == !detail.at("failed").get<bool>());
    }
    for (const auto& risk : output.at("reliability")) {
        const auto p = risk.at("failure").at("probability").get<double>();
        BOOST_TEST(p >= 0.0);
        BOOST_TEST(p <= 1.0);
        BOOST_TEST(risk.at("failure").at("wilson_95").size() == 2u);
    }
    BOOST_TEST(output.dump() == nlohmann::json::parse(dta::assess_json(input.dump())).dump());
}

BOOST_AUTO_TEST_CASE(separate_material_matches_legacy_and_uses_every_parameter) {
    auto input = example();
    const auto material = input.at("material");
    const auto legacy_result = dta::assess_json(input.dump());
    input.erase("material");
    BOOST_TEST(dta::assess_json(input.dump(), material.dump()) == legacy_result);
    const auto parsed = dta::assessment_from_json(input, material);
    BOOST_TEST(parsed.material.paris_c == material.at("paris_c").get<double>());
    BOOST_TEST(parsed.material.paris_m == material.at("paris_m").get<double>());
    BOOST_TEST(parsed.material.toughness == material.at("toughness").get<double>());
    BOOST_TEST(parsed.material.threshold == material.at("threshold").get<double>());
    BOOST_TEST(parsed.material.walker_gamma == material.at("walker_gamma").get<double>());
    BOOST_TEST(dta::parse_assessment(input.dump(), material.dump()).seed == parsed.seed);
    const nlohmann::json changes = {{"paris_c", 0}, {"paris_m", 2},
                                   {"toughness", 70}, {"threshold", 1e6},
                                   {"walker_gamma", 1}};
    for (auto it = changes.begin(); it != changes.end(); ++it) {
        auto changed = material;
        changed[it.key()] = it.value();
        auto legacy = input;
        legacy["material"] = changed;
        const auto result = dta::assess_json(input.dump(), changed.dump());
        BOOST_TEST(result == dta::assess_json(legacy.dump()));
        BOOST_TEST(result != legacy_result);
    }
}

BOOST_AUTO_TEST_CASE(separate_material_rejects_missing_unknown_and_invalid_parameters) {
    auto input = example();
    const auto material = input.at("material");
    input.erase("material");
    BOOST_CHECK_THROW(dta::assessment_from_json(input), nlohmann::json::out_of_range);
    for (auto it = material.begin(); it != material.end(); ++it) {
        auto missing = material;
        missing.erase(it.key());
        BOOST_CHECK_THROW(dta::assessment_from_json(input, missing), nlohmann::json::out_of_range);
        for (const auto& invalid : {nlohmann::json(-1), nlohmann::json("3"),
                                   nlohmann::json(true), nlohmann::json(nullptr),
                                   nlohmann::json::array(), nlohmann::json::object(),
                                   nlohmann::json(std::numeric_limits<double>::infinity()),
                                   nlohmann::json(std::numeric_limits<double>::quiet_NaN())}) {
            auto bad = material;
            bad[it.key()] = invalid;
            BOOST_CHECK_THROW(dta::assessment_from_json(input, bad), std::invalid_argument);
        }
    }
    for (const auto* field : {"material", "units", "schema_version", "unknown"}) {
        auto bad = material;
        bad[field] = 1;
        BOOST_CHECK_THROW(dta::assessment_from_json(input, bad), std::invalid_argument);
    }
    for (const auto* field : {"paris_m", "toughness"}) {
        auto bad = material;
        bad[field] = 0;
        BOOST_CHECK_THROW(dta::assessment_from_json(input, bad), std::invalid_argument);
    }
    auto bad = material;
    bad["walker_gamma"] = 1.01;
    BOOST_CHECK_THROW(dta::assessment_from_json(input, bad), std::invalid_argument);
    bad = material;
    bad["paris_c"] = 0;
    bad["threshold"] = 0;
    bad["walker_gamma"] = 0;
    BOOST_CHECK_NO_THROW(dta::assessment_from_json(input, bad));
    bad["walker_gamma"] = 1;
    BOOST_CHECK_NO_THROW(dta::assessment_from_json(input, bad));
    for (const auto& nonobject : {nlohmann::json(nullptr), nlohmann::json::array(),
                                  nlohmann::json(42), nlohmann::json("material")}) {
        BOOST_CHECK_THROW(dta::assessment_from_json(input, nonobject), std::invalid_argument);
        BOOST_CHECK_THROW(dta::assessment_from_json(nonobject, material), std::invalid_argument);
    }
    auto unknown = input;
    unknown["unknown"] = 1;
    BOOST_CHECK_THROW(dta::assessment_from_json(unknown, material), std::invalid_argument);
    for (const auto& inline_material : {material, nlohmann::json(nullptr)}) {
        input["material"] = inline_material;
        BOOST_CHECK_THROW(dta::assessment_from_json(input, material), std::invalid_argument);
    }
}

BOOST_AUTO_TEST_CASE(both_separate_json_texts_use_strict_bounded_parser) {
    auto input = example();
    const auto material = input.at("material");
    input.erase("material");
    for (const auto& malformed : {"", "{", "null trailing"}) {
        BOOST_CHECK_THROW(dta::parse_assessment(input.dump(), malformed), nlohmann::json::parse_error);
        BOOST_CHECK_THROW(dta::parse_assessment(malformed, material.dump()), nlohmann::json::parse_error);
    }
    const auto duplicated = material.dump().substr(0, material.dump().size() - 1) +
                            ",\"paris_c\":0}";
    for (const auto& invalid : {duplicated, std::string("{\"nested\":{\"x\":1,\"x\":2}}"),
                               std::string(33, '[') + "0" + std::string(33, ']'),
                               std::string(4 * 1024 * 1024 + 1, ' ')}) {
        BOOST_CHECK_THROW(dta::parse_assessment(input.dump(), invalid), std::invalid_argument);
        BOOST_CHECK_THROW(dta::parse_assessment(invalid, material.dump()), std::invalid_argument);
    }
    auto padded = material.dump();
    padded.resize(4 * 1024 * 1024, ' ');
    BOOST_CHECK_NO_THROW(dta::parse_assessment(input.dump(), padded));
    auto padded_input = input.dump();
    padded_input.resize(4 * 1024 * 1024, ' ');
    BOOST_CHECK_NO_THROW(dta::parse_assessment(padded_input, material.dump()));
}

BOOST_AUTO_TEST_CASE(json_rejects_malformed_or_ambiguous_input) {
    BOOST_CHECK_THROW(dta::parse_assessment("{"), nlohmann::json::parse_error);
    BOOST_CHECK_THROW(dta::parse_assessment("{\"a\":1,\"a\":2}"), std::invalid_argument);
    BOOST_CHECK_THROW(dta::parse_assessment(std::string(33, '[') + "0" + std::string(33, ']')),
                      std::invalid_argument);
    BOOST_CHECK_THROW(dta::parse_assessment(std::string(4 * 1024 * 1024 + 1, ' ')),
                      std::invalid_argument);
    auto input = example();
    input["units"]["length"] = "mm";
    BOOST_CHECK_THROW(dta::assessment_from_json(input), std::invalid_argument);
    input = example();
    input["simulaton"] = input["simulation"];
    BOOST_CHECK_THROW(dta::assessment_from_json(input), std::invalid_argument);
    input = example();
    input["fleet"]["airplanes"] = nlohmann::json::object();
    BOOST_CHECK_THROW(dta::assessment_from_json(input), std::invalid_argument);
    input = example();
    input["material"].erase("toughness");
    BOOST_CHECK_THROW(dta::assessment_from_json(input), nlohmann::json::out_of_range);
    input = example();
    input["material"]["paris_m"] = "3";
    BOOST_CHECK_THROW(dta::assessment_from_json(input), std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(json_integer_fields_do_not_truncate_or_wrap) {
    for (const auto& bad : {nlohmann::json(-1), nlohmann::json(1.5),
                           nlohmann::json(true), nlohmann::json(4294967296ULL)}) {
        auto input = example();
        input["simulation"]["trials"] = bad;
        BOOST_CHECK_THROW(dta::assessment_from_json(input), std::invalid_argument);
    }
    auto input = example();
    input["schema_version"] = 1.0;
    BOOST_CHECK_THROW(dta::assessment_from_json(input), std::invalid_argument);
    input = example();
    input["simulation"]["seed"] = std::numeric_limits<std::uint64_t>::max();
    BOOST_TEST(dta::assessment_from_json(input).seed == std::numeric_limits<std::uint64_t>::max());
    input["simulation"]["seed"] = -1;
    BOOST_CHECK_THROW(dta::assessment_from_json(input), std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(json_zero_failures_still_has_sampling_uncertainty) {
    auto input = example();
    input["material"]["threshold"] = 1e6;
    input["material"]["toughness"] = 1e6;
    const auto output = nlohmann::json::parse(dta::assess_json(input.dump()));
    for (const auto& risk : output.at("reliability")) {
        BOOST_TEST(risk.at("failure").at("events").get<unsigned>() == 0u);
        BOOST_TEST(risk.at("failure").at("wilson_95").at(1).get<double>() > 0.0);
    }
}
