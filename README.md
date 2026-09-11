# DTAssessment

DTAssessment is a C++17 multi-crack propagation simulation using CMake and Boost.Program_options.

The current model provides:

- Randomly initialized surface cracks in a rectangular specimen.
- Mode-I stress intensity range using a geometry factor.
- Paris-law crack growth when the stress intensity range exceeds a threshold.
- A simple proximity-based crack interaction factor.
- Crack coalescence when two active cracks approach within the merge distance.
- CSV output suitable for plotting or post-processing.

## Build

Install a C++17 compiler, CMake 3.20+, and Boost.Program_options. Then:

```powershell
cmake -S . -B build
cmake --build build --config Release
```

## Run

```powershell
.\build\Release\dtassessment.exe --cracks 20 --cycles 500 --output results.csv
```

Use `--help` to list all command-line options.

This is an engineering prototype: material properties, boundary conditions, mixed-mode effects,
and mesh-based fracture mechanics can be added as the model is calibrated against test data.
