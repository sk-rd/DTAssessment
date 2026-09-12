import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

import dta


def test_add():
    assert dta.add(2, 3) == 5
    assert dta.add(-5, 3) == -2


def example():
    data = Path(__file__).resolve().parents[1] / "data"
    value = json.loads((data / "example.json").read_text(encoding="utf-8"))
    value["simulation"]["trials"] = 2
    value["simulation"]["max_cycles"] = 5
    material = json.loads((data / "material.json").read_text(encoding="utf-8"))
    return value, material


def invalid_materials(material):
    yield ""
    yield "{"
    yield "[]"
    yield "null"
    yield "42"
    yield json.dumps(material)[:-1] + ',"paris_c":0}'
    yield '{"nested":{"x":1,"x":2}}'
    yield "[" * 33 + "0" + "]" * 33
    yield " " * (4 * 1024 * 1024 + 1)
    for field in material:
        missing = dict(material)
        del missing[field]
        yield json.dumps(missing)
        for invalid in (-1, "3", True, None, [], {}):
            yield json.dumps(dict(material, **{field: invalid}))
    for field in ("schema_version", "units", "material", "unknown"):
        yield json.dumps(dict(material, **{field: 1}))
    for field in ("paris_m", "toughness"):
        yield json.dumps(dict(material, **{field: 0}))
    yield json.dumps(dict(material, walker_gamma=1.01))
    yield json.dumps(material).replace('"paris_c": 4e-09', '"paris_c": 1e999')


def expect_value_error(*args):
    try:
        dta.assess_json(*args)
    except ValueError:
        return
    raise AssertionError("invalid input must raise ValueError")


def test_assess_json():
    value, material = example()
    result_text = dta.assess_json(json.dumps(value), json.dumps(material))
    legacy = dict(value, material=material)
    assert result_text == dta.assess_json(json.dumps(legacy))
    result = json.loads(result_text)
    assert len(result["critical_details"]) == 4
    assert result["simulation"]["trials"] == 2
    assert isinstance(result["critical_details"][0]["censored"], bool)
    expect_value_error("{}")
    expect_value_error(json.dumps(value))
    expect_value_error(json.dumps(legacy), json.dumps(material))
    expect_value_error(json.dumps(dict(value, material=None)), json.dumps(material))
    for bad in ("{}", "[]", "null", "42", "{", '{"a":1,"a":2}',
                "[" * 33 + "0" + "]" * 33, " " * (4 * 1024 * 1024 + 1)):
        expect_value_error(bad)
        expect_value_error(bad, json.dumps(material))
    for bad in invalid_materials(material):
        expect_value_error(json.dumps(value), bad)
    for field, changed in (("paris_c", 0), ("paris_m", 2), ("toughness", 70),
                           ("threshold", 1e6), ("walker_gamma", 1)):
        different = dict(material, **{field: changed})
        separate = dta.assess_json(json.dumps(value), json.dumps(different))
        assert separate != result_text
        assert separate == dta.assess_json(json.dumps(dict(value, material=different)))


def test_cli(executable):
    executable = str(Path(executable).resolve())
    with tempfile.TemporaryDirectory(dir=Path.cwd()) as folder:
        source = Path(folder) / "input.json"
        material_file = Path(folder) / "material.json"
        target = Path(folder) / "output.json"
        source.write_text("{}", encoding="utf-8")
        target.write_text("preserve on invalid input", encoding="utf-8")
        failed = subprocess.run([executable, str(source), str(target)], capture_output=True, text=True)
        assert failed.returncode == 1
        assert "error" in json.loads(failed.stderr)
        assert target.read_text(encoding="utf-8") == "preserve on invalid input"
        same_file = subprocess.run([executable, str(source), str(source)], capture_output=True, text=True)
        assert same_file.returncode == 1
        assert source.read_text(encoding="utf-8") == "{}"
        value, material = example()
        source.write_text(json.dumps(value), encoding="utf-8")
        material_file.write_text(json.dumps(material), encoding="utf-8")
        success = subprocess.run([executable, str(source), str(material_file), str(target)],
                                 capture_output=True, text=True)
        assert success.returncode == 0, success.stderr
        expected = target.read_text(encoding="utf-8")
        assert json.loads(expected) == json.loads(dta.assess_json(json.dumps(value), json.dumps(material)))
        assert len(json.loads(expected)["critical_details"]) == 4
        source.write_text(json.dumps(dict(value, material=material)), encoding="utf-8")
        legacy = subprocess.run([executable, str(source), str(target)], capture_output=True, text=True)
        assert legacy.returncode == 0, legacy.stderr
        assert target.read_text(encoding="utf-8") == expected
        for inline in (material, None):
            source.write_text(json.dumps(dict(value, material=inline)), encoding="utf-8")
            conflict = subprocess.run([executable, str(source), str(material_file), str(target)],
                                      capture_output=True, text=True)
            assert conflict.returncode == 1
            assert "inline material" in json.loads(conflict.stderr)["error"]
            assert target.read_text(encoding="utf-8") == expected
        source.write_text(json.dumps(value), encoding="utf-8")
        omitted = subprocess.run([executable, str(source), str(target)], capture_output=True, text=True)
        assert omitted.returncode == 1
        assert target.read_text(encoding="utf-8") == expected
        for bad in invalid_materials(material):
            material_file.write_text(bad, encoding="utf-8")
            failed = subprocess.run([executable, str(source), str(material_file), str(target)],
                                    capture_output=True, text=True)
            assert failed.returncode == 1
            assert "error" in json.loads(failed.stderr)
            assert target.read_text(encoding="utf-8") == expected
            if len(bad) > 4 * 1024 * 1024:
                assert str(material_file) in json.loads(failed.stderr)["error"]
        material_file.write_text(json.dumps(material), encoding="utf-8")
        for protected in (source, material_file):
            original = protected.read_text(encoding="utf-8")
            aliases = [protected]
            for kind in ("hard", "symbolic"):
                alias = Path(folder) / (protected.stem + "-" + kind + ".json")
                try:
                    if kind == "hard":
                        os.link(protected, alias)
                    else:
                        alias.symlink_to(protected)
                except (OSError, NotImplementedError):
                    continue
                aliases.append(alias)
            for alias in aliases:
                failed = subprocess.run([executable, str(source), str(material_file), str(alias)],
                                        capture_output=True, text=True)
                assert failed.returncode == 1
                assert "different files" in json.loads(failed.stderr)["error"]
                assert protected.read_text(encoding="utf-8") == original
        for bad_input in ("[]", "null", "{", " " * (4 * 1024 * 1024 + 1)):
            source.write_text(bad_input, encoding="utf-8")
            failed = subprocess.run([executable, str(source), str(material_file), str(target)],
                                    capture_output=True, text=True)
            assert failed.returncode == 1
            assert target.read_text(encoding="utf-8") == expected
            if len(bad_input) > 4 * 1024 * 1024:
                assert str(source) in json.loads(failed.stderr)["error"]
        source.write_text(json.dumps(value), encoding="utf-8")
        absent = Path(folder) / "missing-material.json"
        missing_material = subprocess.run([executable, str(source), str(absent), str(target)],
                                          capture_output=True, text=True)
        assert missing_material.returncode == 1
        assert str(absent) in json.loads(missing_material.stderr)["error"]
        assert target.read_text(encoding="utf-8") == expected
        missing = subprocess.run([executable, str(Path(folder) / "missing.json"), str(target)],
                                 capture_output=True, text=True)
        assert missing.returncode == 1
        assert "error" in json.loads(missing.stderr)
        assert target.read_text(encoding="utf-8") == expected
        assert subprocess.run([executable], capture_output=True).returncode == 2


if __name__ == "__main__":
    test_add()
    test_assess_json()
    if len(sys.argv) > 1:
        test_cli(sys.argv[1])
