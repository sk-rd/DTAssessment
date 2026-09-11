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
