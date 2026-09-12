#!/usr/bin/env python3
"""NASGRO/AFGROW-inspired fatigue crack-growth simulation using JSON files."""

import argparse
import json
import math


def positive(name, value):
    if not math.isfinite(value) or value <= 0:
        raise ValueError(f"{name} must be positive and finite")


def geometry_factor(data, crack_length):
    if data.get("geometry", "center_crack") == "edge_crack":
        return 1.12
    return math.sqrt(1.0 / math.cos(math.pi * crack_length / data["width"]))


def simulate(input_data, material):
    geometry = input_data.get("geometry", "center_crack")
    if geometry not in ("center_crack", "edge_crack"):
        raise ValueError("geometry must be center_crack or edge_crack")
    for name in ("width", "initial_crack", "critical_crack",
                 "cycles_per_step", "max_cycles"):
        positive(name, float(input_data[name]))
    for name in ("c", "m", "threshold_delta_k", "fracture_toughness"):
        positive(name, float(material[name]))
    if input_data["initial_crack"] >= input_data["critical_crack"]:
        raise ValueError("initial_crack must be less than critical_crack")
    if input_data["critical_crack"] >= input_data["width"] / 2:
        raise ValueError("critical_crack must be less than width/2")
    if input_data["max_stress"] <= input_data["min_stress"]:
        raise ValueError("max_stress must exceed min_stress")

    def delta_k(crack):
        return (geometry_factor(input_data, crack)
                * (input_data["max_stress"] - input_data["min_stress"])
                * math.sqrt(math.pi * crack))

    def max_k(crack):
        return (geometry_factor(input_data, crack)
                * input_data["max_stress"] * math.sqrt(math.pi * crack))

    history = []
    cycles = 0.0
    crack = input_data["initial_crack"]
    while True:
        dk = delta_k(crack)
        mk = max_k(crack)
        if dk <= material["threshold_delta_k"]:
            rate = 0.0
        elif mk >= material["fracture_toughness"]:
            rate = None
        else:
            rate = material["c"] * dk ** material["m"]
        history.append({"cycles": cycles, "crack_length": crack,
                        "delta_k": dk, "max_k": mk, "growth_rate": rate})
        if mk >= material["fracture_toughness"] or crack >= input_data["critical_crack"]:
            termination = "fracture"
            break
        if cycles >= input_data["max_cycles"]:
            termination = "max_cycles"
            break
        step = min(input_data["cycles_per_step"], input_data["max_cycles"] - cycles)
        crack = min(input_data["critical_crack"], crack + rate * step)
        cycles += step
    return {"termination": termination, "history": history}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("input", help="simulation input JSON")
    parser.add_argument("material", help="material properties JSON")
    parser.add_argument("output", help="output JSON")
    args = parser.parse_args()
    with open(args.input, encoding="utf-8") as file:
        input_data = json.load(file)
    with open(args.material, encoding="utf-8") as file:
        material = json.load(file)
    result = simulate(input_data, material)
    with open(args.output, "w", encoding="utf-8") as file:
        json.dump(result, file, indent=2, allow_nan=False)
        file.write("\n")


if __name__ == "__main__":
    main()
