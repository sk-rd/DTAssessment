# DTA

飞机结构损伤容限与蒙特卡洛可靠性评估的 C++17 原型。提供 JSON 文件命令行入口、
header-only 计算接口和 Boost.Python 接口。

**当前用于演示和研究，不可直接用于适航、维修间隔或结构安全决策。** 本项目不是
NASGRO/DARWIN 的复刻，示例材料参数和几何模型未经工程标定。

## 快速体验

完成下文构建后运行（Windows 多配置生成器的可执行文件在 `build/Release/`）：

```bash
/home/runner/work/DTAssessment/DTAssessment/build/dta_assess \
  /home/runner/work/DTAssessment/DTAssessment/data/example.json \
  /home/runner/work/DTAssessment/DTAssessment/data/material.json \
  /tmp/dta-result.json
```

示例包含一个机队、一架飞机、一个机翼主部件、两个面板和四个裂纹细节，执行
100 次蒙特卡洛试验，每次最多 3000 个循环。输出包含裂纹扩展曲线、剩余强度曲线、
扩展寿命以及各级失效、MSD、MED 的概率和 95% Wilson 置信区间。
命令行成功返回 0，输入/计算/文件错误返回 1 并向 stderr 输出 JSON 错误对象，
参数数量错误返回 2。计算完成前不会打开输出文件；输出不能与评估输入或材料文件是同一个文件。

## 输入与模型

评估输入格式见
[`data/example.json`](/home/runner/work/DTAssessment/DTAssessment/data/example.json)，
材料属性单独存储在
[`data/material.json`](/home/runner/work/DTAssessment/DTAssessment/data/material.json)。
`schema_version` 必须是整数 `1`；除两种损伤判据的计数阈值默认是 `2` 外，
示例中的字段均为必填。未知字段、重复 JSON 键、非法单位和参数会被拒绝。
各级 ID 必须由 1–128 个英文字母、数字、下划线或连字符组成，同级不重复。

### 独立材料文件

命令行按 `dta_assess INPUT.json MATERIAL.json OUTPUT.json` 分别读取评估配置与材料参数。
材料文件的顶层直接包含 `paris_c`、`paris_m`、`toughness`、`threshold`、
`walker_gamma` 五个必填数值字段，不再包裹在 `material` 对象中。同一材料文件可供
多个评估任务复用；当前一个任务中的所有细节仍共用一组材料参数。

材料文件使用下述统一单位，不单独定义单位或进行单位转换。缺失、未知、重复字段、
非数值、非有限数值和不符合物理约束的参数均会报错；不会回退到默认材料参数。
两个输入文件分别受 4 MiB 大小和 32 层 JSON 嵌套限制。

为兼容已有调用，旧式 `dta_assess INPUT.json OUTPUT.json` 和单参数 JSON API
仍支持评估输入中的内嵌 `material` 对象。使用独立材料输入时，评估 JSON 中必须
删除 `material` 字段；同时提供两处材料会报错，不会静默覆盖。

### 物理参数与计算约定

- **单位**：长度 m，应力 MPa，应力强度因子 MPa√m，寿命为循环数，不能混用 mm。
  `paris_c` 的单位为 m/cycle/(MPa√m)^`paris_m`，必须与材料数据标定单位一致。
- **初始裂纹**：每个 critical detail 独立抽取二参数 Weibull 分布，
  `F(a)=1-exp(-(a/scale)^shape)`，无位置偏移、不截断。每次试验代表一个完整机队的
  初始状态抽样；不是将飞机数量当作试验次数。固定 `seed` 在相同实现、输入和遍历
  顺序下可复现，但不同 C++ 标准库的分布实现不保证逐位一致。
- **载荷谱**：`spectrum` 中的载荷块按顺序逐循环执行，每块重复 `cycles` 次，
  到谱末尾后循环播放，直到 `max_cycles`。所有细节当前共用材料和应力谱。
  `maximum>0`、`minimum<maximum`；压缩部分按裂纹闭合处理，不计入张开驱动力。
- **扩展律**：`Kmax=Y(a/W)·sigma_max·sqrt(pi·a)`，
  `R_open=max(0,sigma_min/sigma_max)`，
  `DeltaK_eff=Kmax·(1-R_open)^walker_gamma`。
  当 `DeltaK_eff>threshold` 时，`da/dN=C·DeltaK_eff^m`，否则不扩展。
  `0<=walker_gamma<=1`，其中 `1` 退化为张开部分的 Paris 应力强度因子范围。
  这是 Paris–Walker 简化模型，**不是 NASGRO 裂纹扩展方程**。
  自适应显式子步将单步裂纹增量控制在当前长度的 2% 以内；寿命按整数循环报告，
  不是精确的断裂时刻。谱块的幅值、应力比和顺序影响计算，但尚无过载迟滞或闭合历史模型。
- **剩余强度**：`sigma_res=Kc/(Y·sqrt(pi·a))`，是断裂韧度控制的远场临界应力，
  不包括屈服、塑性塌陷或载荷重分配。达到当前载荷的韧度判据、韧带极限或几何
  适用域边界时终止该细节；初始状态已不满足第一循环条件的裂纹寿命为 0。

### 几何应力强度因子数据库（首版解析条目）

内置数据库按 `geometry` 名称选择条目，适用于均匀远场拉伸的有限宽板，`width=W`
始终为板的**全宽**。这不是通用有限元 SIF 数据库。

| geometry | 裂纹尺寸 `a` | 修正系数 `Y` | 适用域 |
| --- | --- | --- | --- |
| `center_crack` | 中心贯穿裂纹半长，总长 `2a` | `sqrt(sec(pi·a/W))` | `0<a/W<0.5` |
| `edge_crack` | 单边裂纹深度 | `1.12-0.231x+10.55x²-21.72x³+30.39x⁴`，`x=a/W` | `0<a/W<0.6` |

边裂纹多项式不能外推到剩余韧带完全断裂。`geometry_domain` 表示超出模型适用域，
并非已证明结构断裂；本原型保守地将其计为失效。适用域外输出的剩余强度 `0`
也是保守标记，不是塌陷后的物理解。工程应用应补充经验证的几何解、应力梯度、
厚度、孔边/表面裂纹与材料试验数据。

### 五级结构与损伤判据

`fleet → airplanes → major_components → components → critical_details`

- **局部疲劳损伤（LFD）**：独立 critical detail 裂纹的扩展和失效。
- **MSD（多部位损伤）**：同一 component 内，达到终止判据的细节数量不少于
  `msd_min_failed_details`（至少 2）。
- **MED（多元件损伤）**：同一 major component 内，失效 component 数量不少于
  `med_min_failed_components`（至少 2）。一个 component 的多个失效细节不算多个元件。
- 上级失效采用任一子项失效的保守并集；MSD/MED 事件逐级向上汇总。
  计数阈值可以大于子项数量，此时该事件不可能发生。单部件、单细节结构可用于局部损伤分析。

**MSD/MED 当前仅为广布疲劳损伤的统计筛查**：它统计评估期内失效部位的共现，
不等同于真实 WFD 起始寿命或整机解体概率。裂纹相互作用、联结、载荷转移和失效后
结构响应尚未实现；为保留共现统计，某子项失效后其余子项仍按原谱独立计算。
尚未考虑细节之间的随机相关性、裂纹萌生、腐蚀及检修/POD。

## 输出说明

- `critical_details`：仅保存第 0 次试验的逐细节曲线，避免输出全部随机轨迹。
  `history` 同时给出 `cycle`、`crack_size`、`residual_strength`，包含初始点、按
  `history_interval` 抽样的点和终止点。`life_cycles` 为观测到的失效循环；
  若 `censored=true`、`failure_mode="runout"`，则只表示截至 `max_cycles` 未失效，
  **不能将其解释为真实失效寿命或蒙特卡洛平均寿命**。
- `reliability`：每一级节点的完整机队试验计数，`failure`、`msd`、`med` 分别包含
  `events`、`probability`、`wilson_95`。概率分母均为 `trials`；尚非概率随时间曲线。
  未观测到失效不代表风险为零，置信区间只反映抽样误差，不包含模型/材料不确定性。
- `path` 由各级 ID 以 `/` 连接定位节点；`simulation`、`units` 和 `limitations`
  随结果一并保存。

输入文件上限 4 MiB、JSON 嵌套上限 32 层、结构节点最多 10000 个；
`trials<=10000`，`max_cycles<=1000000`，
`细节数×trials×max_cycles<=20000000`，首个试验的曲线最多 200000 点。
数值积分每循环最多 4096 个子步、总共最多 50000000 个子步。达到资源限制或发生
数值溢出会报错而不是计入结构失效。

## 目录结构

- `doc/`：项目文档
- `include/`：对外 `.hpp` 头文件，包含实现
- `src/`：JSON 命令行入口和 Python 绑定
- `test/`：测试代码

## 依赖管理

项目使用 vcpkg manifest 模式，依赖定义在 `vcpkg.json` 中。
测试使用 Boost.Test，JSON 解析/序列化使用 nlohmann-json。

## 文档

项目文档使用 Doxygen 生成。可通过以下命令启用：

```bash
cmake --preset vcpkg -DDTA_BUILD_DOCS=ON
cmake --build /home/runner/work/DTAssessment/DTAssessment/build --target dta_docs
```

仓库已包含 GitHub Actions 工作流 `/home/runner/work/DTAssessment/DTAssessment/.github/workflows/docs-pages.yml`，会在 `main`/`master` 分支推送后自动生成文档并部署到 GitHub Pages。

首次启用时，请在 GitHub 仓库的 Pages 设置中确认 source 为 **GitHub Actions**。

## 构建

先设置 `VCPKG_ROOT` 环境变量，然后执行：

```bash
cd /home/runner/work/DTAssessment/DTAssessment
cmake --preset vcpkg
cmake --build /home/runner/work/DTAssessment/DTAssessment/build --config Release
ctest --test-dir /home/runner/work/DTAssessment/DTAssessment/build -C Release --output-on-failure
```

默认会构建 Boost.Python 模块。构建完成后，可以将生成目录加入
`PYTHONPATH`，然后从 Python 调用接口：

```bash
export PYTHONPATH="/home/runner/work/DTAssessment/DTAssessment/build:$PYTHONPATH"
python3 -c "import dta; from pathlib import Path; print(dta.assess_json(Path('/home/runner/work/DTAssessment/DTAssessment/data/example.json').read_text(), Path('/home/runner/work/DTAssessment/DTAssessment/data/material.json').read_text()))"
```

如不需要 Python 接口，可以通过 `-DDTA_BUILD_PYTHON_BINDINGS=OFF` 关闭。
`dta.assess_json(input_text, material_text)` 接受评估和材料两个 JSON 字符串，
返回结果 JSON 字符串；错误转换为 Python `ValueError`。字符串接口不读取文件，
由调用方读取各自文件后传入。原有单字符串接口仅用于内嵌材料的旧式输入。
C++ 对应提供 `assessment_from_json(input, material)`、
`parse_assessment(input_text, material_text)` 和 `assess_json(input_text, material_text)`
双参数重载，并保留原有单参数接口。
原有 `dta.add` 接口保留。

仅生成文档、不安装计算依赖时，请同时设置 `-DBUILD_TESTING=OFF`、
`-DDTA_BUILD_PYTHON_BINDINGS=OFF` 和 `-DDTA_BUILD_CLI=OFF`。
