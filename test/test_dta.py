import json
from pathlib import Path
import subprocess
import sys
import tempfile

import dta


def test_add():
    assert dta.add(2, 3) == 5
    assert dta.add(-5, 3) == -2


def test_assess_json():
    source = Path(__file__).resolve().parents[1] / "data" / "example.json"
    value = json.loads(source.read_text(encoding="utf-8"))
    value["simulation"]["trials"] = 2
    value["simulation"]["max_cycles"] = 5
    result = json.loads(dta.assess_json(json.dumps(value)))
    assert len(result["critical_details"]) == 4
    assert result["simulation"]["trials"] == 2
    assert isinstance(result["critical_details"][0]["censored"], bool)
    try:
        dta.assess_json("{}")
    except ValueError:
        pass
    else:
        raise AssertionError("invalid input must raise ValueError")


def test_cli(executable):
    with tempfile.TemporaryDirectory() as folder:
        source = Path(folder) / "input.json"
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
        value = json.loads((Path(__file__).resolve().parents[1] / "data" / "example.json")
                           .read_text(encoding="utf-8"))
        value["simulation"]["trials"] = 2
        value["simulation"]["max_cycles"] = 5
        source.write_text(json.dumps(value), encoding="utf-8")
        success = subprocess.run([executable, str(source), str(target)], capture_output=True, text=True)
        assert success.returncode == 0, success.stderr
        assert len(json.loads(target.read_text(encoding="utf-8"))["critical_details"]) == 4
        missing = subprocess.run([executable, str(Path(folder) / "missing.json"), str(target)],
                                 capture_output=True, text=True)
        assert missing.returncode == 1
        assert "error" in json.loads(missing.stderr)


if __name__ == "__main__":
    test_add()
    test_assess_json()
    if len(sys.argv) > 1:
        test_cli(sys.argv[1])
