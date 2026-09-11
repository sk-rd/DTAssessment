# DTA

一个使用 CMake 和 vcpkg 的基础 C++ 项目模板，参考 Boost 风格组织头文件，包含 `doc`、`include`、`src`、`test` 目录。

## 目录结构

- `doc/`：项目文档
- `include/`：对外 `.hpp` 头文件，包含实现
- `src/`：预留给非头文件化扩展
- `test/`：测试代码

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
