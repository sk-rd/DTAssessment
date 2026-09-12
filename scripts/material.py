def validate_material(material):
    for name in ("c", "m", "threshold_delta_k", "fracture_toughness"):
        if material[name] <= 0:
            raise ValueError(f"{name} must be positive")
