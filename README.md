# dta

一个使用 CMake 和 vcpkg 的基础 C++ 项目模板，包含 `doc`、`include`、`src`、`test` 目录。

## 目录结构

- `doc/`：项目文档
- `include/`：对外头文件
- `src/`：源代码实现
- `test/`：测试代码

## 依赖管理

项目使用 vcpkg manifest 模式，依赖定义在 `vcpkg.json` 中。

## 构建

先设置 `VCPKG_ROOT` 环境变量，然后执行：

```bash
cmake --preset default
cmake --build --preset default
ctest --preset default
```
