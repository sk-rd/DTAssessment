# DTAssessment

DTAssessment 是 **Damage Tolerance Assessment（损伤容限评估）** 的缩写。
项目提供一个面向裂纹扩展和剩余寿命评估的 NASGRO/AFGROW 风格基础程序。

一个使用 CMake 和 vcpkg 的基础 C++ 项目模板，参考 Boost 风格组织头文件，包含 `doc`、`include`、`src`、`test` 目录。

## 目录结构

- `doc/`：项目文档
- `include/`：对外 `.hpp` 头文件，包含实现
- `src/`：命令行程序和 Python 绑定
- `scripts/`：Python 的材料、裂纹扩展、损伤容限评估和 JSON I/O 模块
- `test/`：测试代码

核心功能按领域拆分为独立模块：

- `include/dta/material.hpp`：材料属性与校验
- `include/dta/crack_growth.hpp`：裂纹几何、应力强度因子和扩展速率
- `include/dta/assessment.hpp`：损伤容限评估循环
- `include/dta/json_io.hpp`：JSON 文件读写

## 依赖管理

项目使用 vcpkg manifest 模式，依赖定义在 `vcpkg.json` 中。
测试使用 Boost.Test。

## 文档

项目文档使用 Doxygen 生成。可通过以下命令启用：

```bash
cmake --preset default -DDTA_BUILD_DOCS=ON
cmake --build --preset default --target dta_docs
```

仓库已包含 GitHub Actions 工作流 `/home/runner/work/DTAssessment/DTAssessment/.github/workflows/docs-pages.yml`，会在 `main`/`master` 分支推送后自动生成文档并部署到 GitHub Pages。

首次启用时，请在 GitHub 仓库的 Pages 设置中确认 source 为 **GitHub Actions**。

## 构建

先设置 `VCPKG_ROOT` 环境变量，然后执行：

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default
```

默认会构建 Boost.Python 模块。构建完成后，可以将生成目录加入
`PYTHONPATH`，然后从 Python 调用接口：

```bash
export PYTHONPATH="$PWD/build:$PYTHONPATH"
python3 -c "import dta; print(dta.add(2, 3))"
```

如不需要 Python 接口，可以通过 `-DDTA_BUILD_PYTHON_BINDINGS=OFF` 关闭。

## JSON 裂纹扩展模拟

`dta_simulator` 和 `scripts/dta_simulator.py` 都读取三个 JSON 文件：

```bash
python3 scripts/dta_simulator.py data/input.json data/material.json output.json
```

输入和材料属性使用 SI 长度单位（m）、应力单位 MPa，输出包含每个计算步的裂纹长度、
应力强度因子和裂纹扩展速率。该模型提供 NASGRO/AFGROW 风格的几何因子、阈值和断裂
韧度判据，材料参数中的 `c` 和 `m` 定义 Paris 扩展关系。
