#pragma once

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

namespace dta {

struct PodPoint {
    double crack_length{};
    double probability{};
};

struct NDI {
    std::string name_zh;
    std::string name_en;
    double threshold_cycles{};
    double threshold_hours{};
    double interval_cycles{};
    double interval_hours{};
    std::vector<PodPoint> pod;

    double probability_of_detection(double crack_length) const {
        if (pod.empty()) return 0.0;
        if (crack_length <= pod.front().crack_length) return pod.front().probability;
        for (std::size_t i = 1; i < pod.size(); ++i) {
            if (crack_length <= pod[i].crack_length) {
                const auto& left = pod[i - 1];
                const auto& right = pod[i];
                const double fraction = (crack_length - left.crack_length) /
                                        (right.crack_length - left.crack_length);
                return left.probability + fraction *
                    (right.probability - left.probability);
            }
        }
        return pod.back().probability;
    }
};

} // namespace dta
