import math


def geometry_factor(data, crack_length):
    if data.get("geometry", "center_crack") == "edge_crack":
        return 1.12
    return math.sqrt(1.0 / math.cos(math.pi * crack_length / data["width"]))


def crack_values(data, material, crack_length):
    factor = geometry_factor(data, crack_length)
    delta_k = factor * (data["max_stress"] - data["min_stress"]) * math.sqrt(math.pi * crack_length)
    max_k = factor * data["max_stress"] * math.sqrt(math.pi * crack_length)
    if delta_k <= material["threshold_delta_k"]:
        rate = 0.0
    elif max_k >= material["fracture_toughness"]:
        rate = None
    else:
        rate = material["c"] * delta_k ** material["m"]
    return delta_k, max_k, rate
