import math
from crack_growth import crack_values
from material import validate_material


def assess_damage_tolerance(data, material):
    if data.get("geometry", "center_crack") not in ("center_crack", "edge_crack"):
        raise ValueError("geometry must be center_crack or edge_crack")
    for name in ("width", "initial_crack", "critical_crack", "cycles_per_step", "max_cycles"):
        if not math.isfinite(data[name]) or data[name] <= 0:
            raise ValueError(f"{name} must be positive and finite")
    validate_material(material)
    if data["initial_crack"] >= data["critical_crack"] or data["critical_crack"] >= data["width"] / 2:
        raise ValueError("crack lengths must satisfy 0 < initial_crack < critical_crack < width/2")
    if data["max_stress"] <= data["min_stress"]:
        raise ValueError("max_stress must exceed min_stress")

    history, cycles, crack = [], 0.0, data["initial_crack"]
    while True:
        delta_k, max_k, rate = crack_values(data, material, crack)
        history.append({"cycles": cycles, "crack_length": crack, "delta_k": delta_k,
                        "max_k": max_k, "growth_rate": rate})
        if max_k >= material["fracture_toughness"] or crack >= data["critical_crack"]:
            return {"termination": "fracture", "history": history}
        if cycles >= data["max_cycles"]:
            return {"termination": "max_cycles", "history": history}
        step = min(data["cycles_per_step"], data["max_cycles"] - cycles)
        crack = min(data["critical_crack"], crack + rate * step)
        cycles += step
