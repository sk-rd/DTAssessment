import json
from assessment import assess_damage_tolerance


def run(input_path, material_path, output_path):
    with open(input_path, encoding="utf-8") as file:
        data = json.load(file)
    with open(material_path, encoding="utf-8") as file:
        material = json.load(file)
    with open(output_path, "w", encoding="utf-8") as file:
        json.dump(assess_damage_tolerance(data, material), file, indent=2)
        file.write("\n")
