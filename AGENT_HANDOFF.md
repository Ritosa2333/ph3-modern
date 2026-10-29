# ph3-modern：零上下文接续交接

更新时间：2026-10-07。状态：**WIP，真机 GPU 卡顿与显示密度修复已更新 ph3 test 入口。**

## 2026-10-06 当前工作

最新前沿是 [`portable/MOBILE_RENDERER_20261007.md`](portable/MOBILE_RENDERER_20261007.md)。
用户实际手机反馈主菜单/游戏极卡后，授权 USB 手机测试。RMX5080/Android16、
Chrome/WebView154、Mali-G720 的主菜单大 VBO 子更新路径仅 ~6 FPS，原逐次上传
真机反复 A/B 恢复 ~60 FPS。正式 vertex27 已过 15 项检查和 44,676 tick/11图覆盖
回归并发布到两个既有 ph3 test 入口。其 generation 为
`c658ba224cd131980fd2f1938b7f4cb0ea31df81573cfb3e073fbb710ef80a40`。
用户随后要拔手机，已明确可以拔，不再继续手机操作。display29 的高 DPI/横竖屏
窗口修复已完成标准/verify 55/55、桌面 DPR3.5 尺寸验证与 44,676 tick/11 图回归。
00:50:53 SGT 两个 ph3 test 入口已更新到 display29，最新 generation 为
`cfaf6ef6ba7e361f655dd7e5f5e8860e4e31b8dc805fcda79240e951fceb27f9`，
上一版 vertex27 可回退。用户需刷新后确认手机效果。不要把临时 A/B、覆盖段回归
或桌面 DPI 模拟当作 display29 的真机高难/长时间验收。仍未提交、推送。

用户随后追问停止依据，优化已继续。目标仍是手机基本可玩，99% 量化验收已撤销，
不能以局部回归通过当作整个性能目标完成。新进展从新报告的“继续优化”节开始。
最新用户回复“正常 别在意replay问题”和“不用 想停停”：不调查既有回放偏差，
拒绝本轮连接手机并允许收尾。完成当前桌面回归后停止这一轮，不再追问手机连接。
后续继续需按新消息决定范围，不自动部署或声称手机目标已验收。
当前源码额外包含诊断指令计数、参数直传、内置函数快路径、对象查询和提前算术判定，最新已验证产物及未验项
以新报告为准，不要直接继承下面前一轮 `mobile-final` 的运行结果。
本轮最新产物是 `build/web-wasm-exceptions-loadguards26` 及对应 verify 构建，
两者均 55/55 编译链接，15 项无需资源检查通过。44,676 覆盖 tick / 11 张图
与旧参考一致。4× Hard 六面覆盖段每秒 P95 中位数 17.05 ms，密集段 16.5 ms，
仍有长帧，未证明稳定 60 Hz。本轮已按用户意愿收尾，未提交、推送或部署。
用户随后明确要求“更新test站试试”，已于 23:00:49 SGT 更新
`https://test.touhou.vip/hsob/` 与 `/hsob-perf/` 到相同的 loadguards26 标准版。
新 Runtime generation 为 `84164c809dc0411c3c85d6d67bb129c1bc474573826951c0ed1826e31255c519`。
公开哈希/MIME/缓存指针及两入口实际 Hard 六面启动超过 600 帧均通过，旧版可回退。
其他游戏清单、原资源、字体与存储身份未变。仍未提交或推送；详情见新报告的发布节，
本次部署授权不扩展到主站、GitHub 或后续自动发布。

用户已明确恢复手机优化，先要求桌面 Chromium **4× CPU 降速**，后来确认本机
同时有其它高负载任务，撤销 99% 验收要求并要求看情况收尾。
最终设备目标仍是 RMX5080 / Android WebView 150 以上。代码在本树直接修改，
未提交、推送或部署。新进展与负面证据见
[`portable/MOBILE_PERFORMANCE_20261006.md`](portable/MOBILE_PERFORMANCE_20261006.md)。
99% 的 60 Hz 目标尚未通过，不得把下方 10-01 的源码发布验收当成此目标达成。

含全部改动的 mobile21-strings 已验证一二面 29,577 tick/六张图与六面已覆盖段
15,099 tick/五张图，与旧参考一致。字符串缓存的交替微基准显示其它热路径退步，
所以最终默认关闭它和 SIMD，仅保留实验开关。最终默认构建同样通过上述两组
44,676 tick / 11 张图的覆盖段回归，标准版/验证版均 55/55 编译并链接。
最终产物为 `build/web-wasm-exceptions-mobile-final` 与
`build/web-verify-wasm-exceptions-mobile-final`，收尾结果见新报告。
无需资源检查已扩展至 15/15 PASS。原 Replay01 是 **Easy**，新发现的 Replay14 是
**Hard 六面练习**，引擎录像 stage index 为 1。六面旧录像提前结束仍未修复。

## 0. 先看这几句话

本项目是原版 Touhou Danmakufu ph3 的现代化 / Web 移植。保留原版解析器、
脚本虚拟机、ScriptManager 与游戏逻辑，不是针对 HSoB 重写的精简解释器。
主要实测作品是 Hollow Song of Birds 1.12（HSoB）。**浏览器入口、打包、
资源准备、存档目录还带有 HSoB 专用约定，不能宣称任意 ph3 游戏直接导入即用。**

用户先要求解决手机见 Boss / 密集弹幕卡顿及六面道中闪退，随后把当前任务
切换为：核对通用性、合并引擎工作树、上传 GitHub `ph3-modern`、README 标明
WIP、写好交接。本次不是线上发布。下一位 agent 先读最新用户消息，确认是继续
性能、通用化还是发布维护，不要把旧的性能目标自动当成新的部署授权。

**内存确实改善；Boss 卡顿没有被证明解决；真机验收仍欠缺。**
2026-10-06 用户已明确“正常 别在意replay问题”，停止调查既有回放偏差，继续性能优化。
保留回放证据与覆盖限制，不把这个偏差当作性能工作的阻塞项。

## 1. 入口、分支与合并范围

当前合并后的源码入口：

```text
D:/workspace/eagler/worktrees/ph3-modern
branch: main
repository: YomotsuHisami/ph3-modern
```

关键历史：

| 修订 | 含义 |
| --- | --- |
| `5c92f28e17b6ef3721a3ac4caa9de3af3164b51e` | 原版 ph3 游戏逻辑基线 |
| `900aa3ebeb329187e8c2852b8ad41caa5200ca9e` | Woo DAT/zlib 机制来源，不是全部游戏逻辑来源 |
| `9b844d6` | 原 `perf/full-run` 优化 |
| `0e94eec` | 原 `portable-web-open` 已合并上述性能分支 |
| `bad2017` | 原 HSoB Eagler adapter 提交 |
| `eda746a1042d6128d4eea15e9a3ecd9051d71dda` | 收拢此前未提交适配、uniform16 累积优化、测试和 WIP 文档 |
| `8cea7dfe3128e0de67f8818bf19415f03748c791` | `main` 合并点；父提交是 `0e94eec` 和 `eda746a` |

本交接文件位于合并点之后的文档提交。用 `git rev-parse HEAD` 获取真正当前版本，
不要把日期文档中的旧 HEAD 当成最新状态。合并点的文件树与 `eda746a` 相同。

GitHub 仓库初次建立为 **private**；未创建 Release、上传运行包或部署站点。
原机器上的多个工作树共享 Git 仓库：`origin` 仍然是
`james7132/Danmakufu-ph3`，新增远端 **`ph3-modern`** 才是用户的仓库。
在这台机器推送用 `git push ph3-modern main`，不要向原作者仓库推送。
从用户 GitHub 仓库新克隆时，`origin` 则自然指向用户仓库。

其他目录保留，没有删除或挪走：

```text
eagler/worktrees/hsob-eagler                 旧 adapter 树；历史 build/ 证据在这里
eagler/fangame/engines/ph3-open-web          旧 portable-web-open 参考
eagler/fangame/engines/ph3-open-web-perf     已并入历史的 perf/full-run 参考
eagler/fangame/engines/ph3-original          原版源码参考，不是当前开发入口
eagler/worktrees/eagler-touhou-hsob          独立 Launcher 仓库，不属于本次引擎合并
```

旧工作树存在上游 ExRumia 示例图片的乱码文件名 D/?? 对；它们不是本次删除和新增，
没有收入提交。旧 `_temp/` 和 `HSoB paused 2026-09-30` stash 也保留，**不要再套用
旧 stash**。新 main 工作树用本地 sparse-checkout 排除 `bin_th_dnh/`、`bin_ext/`
的检出，避免继承的非 UTF-8 文件名干扰 Windows；这些上游文件仍在 Git 历史中。
这不是删除游戏数据，也不是缺失源码。不要用 reset/clean 来处理这个现象。

## 2. 通用性：能说什么，不能说什么

`source/GcLib/gstd/Script.cpp/.hpp` 仍是原版语法解析、字节码与 VM。
本次标量内联、指令布局、分配器、环境复用和算术快路径是通用实现。
算术快路径依据原生函数身份及真实操作数类型选择；非数值操作回到原实现，
不是依靠 HSoB 的脚本名或特定弹幕内容走捷径。

无需游戏资源的测试覆盖标量位模式/类型身份、字符串、嵌套数组、写时复制、
可写引用、字面量池扩容、循环/分支、函数返回、task/yield、事件重入、
暂停恢复、终止、异常展开及容器生命周期。六种 VM 配置输出一致。
这证明了这些具体回归用例，不代表已经运行了所有正常脚本。

仍然存在的适配边界：

- `portable/web/shell.mjs` 的 game id、数据和 Replay 路径以及导出函数名是 HSoB adapter。
- `portable/serve-local.py`、`compile-effects.py` 默认查找 `script/th_hsob`。
- `StgStageController.cpp::PrecompileStageScripts` 的 `stageN` 目录及
  `BGTEST.dnh` / `Damage.dnh` 排除是预热启发式，不是语法拒绝规则。通用化时应移到
  作品配置，不能因此删除正常按需加载。
- D3D9/FX 到 WebGL2 的转换只接受已实现的格式与状态；`.pfx` sidecar 必须匹配
  原 FX 源码。任意 FX、额外 pass、方言/引擎扩展不能默认为支持。
- 没有 ph3sx 等方言全面兼容证据，也没有其他作品的完整验收矩阵。
- 原生 Windows 工程本轮没有重建；部分通用 C++ 容器实现也有变更，不要声称
  原生构建完全未受影响或原版旧工具链必定能编译。

## 3. 合并中已有的优化

| 区域 | 主要实现 / 文件 |
| --- | --- |
| 脚本 VM | `Script.*`，`BoundedAllocationCache.hpp`；融合调度、有限分配缓存、环境/栈复用、保留可写句柄语义的标量内联、字面量池和数值算术快路径 |
| 指令存储 | Web32 `code` 从 24 字节压到 16 字节；不是整个进程减少三分之一 |
| 绘制后端 | `portable/DxDevice.cpp`，`StateTable.hpp`、`VertexStream.hpp`、`ScreenVertex.hpp`、`DrawTransforms.hpp`、`FixedModulate.hpp`；合批、状态和常量缓存、顶点转换/上传复用 |
| 绘制对象队列 | `ListNodePool.hpp` 和 `DxScript.*`；复用链表节点，仍立即释放对象引用，保留顺序和迭代器语义 |
| 特效参数 | `Effect.cpp` / `UniformValueCache.hpp`；按 program/location、操作类型、数量和位模式缓存实际上传值；纹理反馈和 alpha test 写入次序未跳过 |
| 跨关缓存 | `PrewarmCacheScope.hpp`、`ScriptClient.*`、`StgStageController.cpp`；换关退休仅剩缓存持有的旧预热脚本，保护活跃客户端，保留当前关卡预热 |
| 纹理内存 | `Texture.cpp/.hpp`、`BudgetLru.hpp`；约 64 MiB 的机会性缓存预算，不驱逐仍被使用的纹理或渲染目标；活跃集合允许超过预算 |
| 协作加载 | `Texture::GetD3DTexture`、`StgEnemyBossSceneObject::Activate` 和 FileManager；Web 无加载线程时不再用 Sleep 等待必需加载 |
| 测量 | 浏览器内有界采样、真实逻辑频率/呈现频率、独立 GL 逻辑分配统计；诊断和普通版计时隔离 |

保留完整 `img.dat`、独立 OGG、实际敌弹/RNG/判定和原输入逻辑。
呈现限帧必须仍可选，不能通过强制锁帧、无敌、减弹或慢逻辑冒充优化。

常用 A/B 开关：`PH3_LEGACY_SCRIPT_SCALARS`、`PH3_LEGACY_SCRIPT_ARITHMETIC`、
`PH3_LEGACY_SCRIPT_CODE_LAYOUT`、`PH3_LEGACY_STAGE_SCRIPT_CACHE`、
`PH3_LEGACY_EFFECT_UNIFORMS`。其他细粒度开关见对应头文件；每次只改一层。

## 4. 验证账本：不要把局部 PASS 外推

### 本次 main 合并后的检查

`node portable/check-wip.mjs` 在合并点 `8cea7df` 的新工作树中重新执行：
**13/13 gate PASS，六种 VM 输出 SHA-256 一致**。包含 optimized/legacy 的
JS/Wasm 异常模式、旧指令布局、VM/平台 AddressSanitizer、容器和观测器测试。
报告在当前树 `build/check-wip/report.json`，日志同目录；输出都不需要 HSoB 数据。

完整标准 Web 构建另行执行 JS 和 Wasm 两种异常模式，具体结果见本文件末尾
“发布检查结果”。构建、无需游戏资源的回归、浏览器游戏、真机验收是四种不同证据。

### 旧 uniform16 游戏证据

以下路径均相对于 **旧树 `../hsob-eagler/build/`**，不会随 GitHub 克隆自动提供：

| 目录 | 准确结果 |
| --- | --- |
| `verify-prefix12-uniform16-20261001` | 原 Replay01 一二面 29,577 tick 与既有参考相同，6 张图通过；两关结束帧/分数也符合录像 |
| `verify-stage6-uniform16-20261001` | 已覆盖的 15,099 tick 和 5 张图与参考相同；实际在 15,098 帧耗尽生命，录像预期 34,829 帧。不是六面完整 PASS |
| `verify-full-replay05-code14-fast32-20261001` | 550 秒诊断窗口结束；最后 end 快照六面 20,247 帧，Wasm 679.3125 MiB。无全程完成证明 |
| `verify-full-replay05-uniform16-fast32-20261001` | **FAIL**：`Attempted to use detached Frame`；最后可用状态四面 5,989 帧，275.375 MiB。sampling/evidence drain 也失败，需要重跑并排查浏览器生命周期，不能伪称运行通过或认定是手机闪退根因 |

已覆盖段 trace SHA-256：

```text
Replay01 stages 1/2: 24d1a50ce6955e8821bfe46f4ef51a352c3d8574e9bc46f3641c62a5a2950c4f
Replay01 stage 6 partial: 79cfb090eb6b1f81f7d7700fb6b1a1f9562a9b88d0fccd65c2704aed01469ea4
```

截图比较只排除 `[980,790,1080,815]` 的 FPS 数字矩形；完整图像差异仍在报告中。
不要扩大遮罩掩盖误差。历史参考是 `verify-prefix12-reference` 和
`verify-stage6-reference`，不能拿新候选重新生成期望值给自己放行。

### 内存和 Boss 性能

相同 Replay05、相同区间，code14 → stagecache15 的 Wasm high-water：
三面 10,000–13,999：396.625 → 229.4375 MiB；四面同区间：476 → 275.375 MiB；
五面 27,000–27,999：679.4375 → 330.5 MiB，约下降 51.4%。
证据是 `mobile-optimization-evidence/stagecache15-matched-stage-memory.json`。
这是 Wasm 内存，不是 Android 进程总 RAM；不能直接把 JS/Wasm 计数相加。

全程逐帧比较中，stagecache15 的旧窗口到五面 28,607 帧截断，和 code14 在此前
记录一致；严格比较仍为 FAIL，因为记录不完整且末端不同。不能裁剪后改称全程通过。

最近一组普通标准版四倍 CPU throttle 的 Boss 指标：arithmetic11 重测
126.4 ms、stagecache15 127.35 ms。这是“每秒窗口的呈现 callback P95 的中位数”，
不是所有帧合并后的 P95；**没有证实 Boss 改善**。
证据：`mobile-optimization-evidence/continuation15-pressure.json`。
uniform16 尚无新的无诊断干扰普通版 Boss 性能结论。

曾在六面约 10,229 帧复现过阻塞加载异常，之后部分本地测试越过该点。
这不能证明用户报告的全部闪退来源已排除。上次 `adb devices -l` 没有设备；
没有 RMX5080 WebView / 用户手机上的完成验收。

## 5. 第一轮接手操作与无需游戏资源的检查

```powershell
cd D:/workspace/eagler/worktrees/ph3-modern
git status --short
git log -5 --oneline
git remote -v
$env:EMSDK = 'D:/workspace/eagler/toolchains/emsdk'
node portable/check-wip.mjs
if ($LASTEXITCODE) { throw 'WIP regression failed; inspect build/check-wip/' }
```

本机版本：Node `24.13.0`，Python `3.14.3`，Emscripten `6.0.6-git`
(`ce75e06884093bcefb86a6b8fd56a5d62a4cc245`)。这不是所有版本的最低要求。
外部克隆设置自己的 `EMSDK`；`PH3_PYTHON` 可指定 Python。VM 测试已去除对
旧 `build/web-wasm-exceptions-stage6-opt/build.json` 的依赖，不必复制历史 build/。

新 Windows 克隆可先仅检出源码，绕开上游示例的乱码文件名：

```powershell
git clone --sparse https://github.com/YomotsuHisami/ph3-modern.git
cd ph3-modern
git sparse-checkout set --no-cone '/*' '!/bin_th_dnh/' '!/bin_ext/'
```

在原 workspace 阅读相应主 playbook：`eagler-touhou/docs/playbooks/performance-rendering.md`，
改 Replay/时序时另读 `replay-determinism.md`，改适配时读 `ADAPTING_A_GAME.md`。
外部克隆至少保留本交接中的逻辑、资源及证据边界，不依赖记忆猜路径。

## 6. 构建、资源与浏览器复现

```powershell
$env:PH3_EXTRA_FLAGS = '["-O3","-flto"]'
node portable/build-web.mjs --exceptions=wasm --label=next
if ($LASTEXITCODE) { throw 'standard build failed' }
node portable/build-web.mjs --variant=verify --exceptions=wasm --label=next
if ($LASTEXITCODE) { throw 'verify build failed' }
```

默认 `--exceptions=js` 仍可用；目录分别为 `build/web[-verify][-wasm-exceptions]-<label>`。
`standard` 没有 Replay trace/boundary；`verify` 有逐 tick 观测和 GL 错误检查；
`boundary` 是较轻的阶段边界诊断。标准计时不可用 verify / fast / CPU profiler / GL 内存观察器。

本机私有依赖（没有提交到 GitHub）：

```text
游戏: D:/workspace/eagler/fangame/games/th_hsob_ver1.12/dnh
录像: 上述目录/script/th_hsob/replay/Main_replay01.dat 和 Main_replay05.dat
字体: C:/Windows/Fonts/msgothic.ttc（只在本机使用，不要复制进仓库/交接包）
着色器工具: D:/workspace/eagler/toolchains/ph3-shaders/
  glslang/bin/glslang.exe
  cross-build/Release/spirv-cross.exe
Launcher + puppeteer-core: D:/workspace/eagler/worktrees/eagler-touhou-hsob
```

`compile-effects.py` 和默认预览仍需此 HSoB/工具布局。新克隆没有这些文件时，
无需游戏资源的检查可以先跑；不要拿联网下载游戏素材或字体代替用户资源授权。

```powershell
$env:HSOB_LAUNCHER = 'D:/workspace/eagler/worktrees/eagler-touhou-hsob'
python portable/compile-effects.py --game-root D:/workspace/eagler/fangame/games/th_hsob_ver1.12/dnh
```

该命令把 sidecar 写到**当前树** `build/effects/`，原游戏文件只读。
截图比较使用 Python `numpy`、`Pillow`；profile 使用 Launcher 的 `puppeteer-core`，
`HSOB_CHROME` 可指定浏览器路径。依赖不存在时先如实报告，不把缺资源当引擎失败。

从原菜单跑一二面并对照（新输出名，勿覆盖旧参考）：

```powershell
node portable/run-campaign-local.mjs --runtime=build/web-verify-wasm-exceptions-next --port=8873 --stage=1 --fast=8 --trace --trace-stages=2 --seconds=300 --capture-frames=1:600,1:8500,1:10500,1:12500,2:600,2:11000 --out=build/verify-prefix12-next
python portable/compare-campaign-evidence.py ../hsob-eagler/build/verify-prefix12-reference build/verify-prefix12-next --ignore-fps-rect=980,790,1080,815
```

六面已覆盖段回归（仍不是完整六面）：

```powershell
node portable/run-campaign-local.mjs --runtime=build/web-verify-wasm-exceptions-next --port=8873 --stage=6 --fast=8 --trace --trace-stages=1 --seconds=180 --capture-frames=3500,9000,10400,12000,14000 --out=build/verify-stage6-next
python portable/compare-campaign-evidence.py ../hsob-eagler/build/verify-stage6-reference build/verify-stage6-next --ignore-fps-rect=980,790,1080,815
```

连续 Replay05 / 内存与稳定性（诊断，不测 FPS）：

```powershell
node portable/run-campaign-local.mjs --runtime=build/web-verify-wasm-exceptions-next --port=8873 --stage=1 --fast=8 --trace --trace-stages=6 --seconds=1200 --fixture=D:/workspace/eagler/fangame/games/th_hsob_ver1.12/dnh/script/th_hsob/replay/Main_replay05.dat --memory --out=build/verify-full-replay05-next
```

跨关运行可能超过单个 MCP 命令上限。**不要把宿主强制终止、detached Frame、
time-window-complete 或无样本当成完整成功。** 不要并行启动多个抢占 8873 的服务，
不要同时进行编译和 A/B 计时。`run-campaign-local.mjs` 会管理单次探针的服务生命周期；
命令中断后先确认自己启动的进程/端口，不能杀其他 agent 的任务。

普通版 Boss 性能：

```powershell
node portable/run-campaign-local.mjs --runtime=build/web-wasm-exceptions-next --port=8873 --stage=1 --throttle=4 --until-frame=12800 --seconds=420 --out=build/perf-stage1-next
python portable/summarize-campaign-runs.py build/perf-stage1-next --out=build/perf-stage1-next-summary.json
```

基线用旧树 `build/web-wasm-exceptions-mobile-arithmetic11-fixed` 或
`web-wasm-exceptions-mobile-stagecache15`，候选用新树标准构建，**顺序**跑，
最好 A/B/A；记录机器/浏览器/热状态，先确认整个 Boss 区间有样本。
CPU throttle 只是本机压力实验，不代表某一款手机。

## 7. 下一步优先级

首先解决 uniform16 连续录像的 detached-frame 验证缺口并保留失败证据；区分
宿主任务生命周期、浏览器退出/context lost、JS/Wasm 异常和真实游戏结束。
不要将此前四面中断的报告改成 PASS；使用独立目录重新收集。

接着完成普通版 Boss A/B 测量，再按采样结果定位。旧采样显示 VM advance、
Draw/RenderToTexture、对象准备和顶点上传仍有显著开销；采样结果是线索，
不等于 uniform16 当前瓶颈已经被证明。不要继续只凭减少 API 数量宣称提升。

对用户六面闪退，要做从一面走来的内存/资源生命周期检查和实体手机试验。
单独直进六面掩盖跨关累计问题；直接内存计数不等于驱动 residency。
完整游戏准确性还欠原版长录像差异定位，手机没崩也不代表录像确定性正确。

若用户转向通用 ph3 运行器，先抽出入口/作品清单、资源/存档/FX 目录和预热策略，
再增加不依赖 HSoB 的普通脚本与多作品语料。不要为通用化改原版语法语义。

## 8. 收尾约束

本轮只合并和上传引擎源码，独立 Launcher 的未提交改动没有收入 ph3-modern。
禁止把此前测试站/正式站的规则当成本次部署授权；没有创建任何后台性能监控任务。
新 clone 的 `build/` 为空是正常情况。日期报告中的绝对路径是历史证据定位，
不是共享机器上永远成立的配置。

每完成一项：记录差异、运行相应 gate、补充证据范围再提交。提交前检查
`git -c core.whitespace=cr-at-eol diff --check`，不要收入 game data、字体、密钥、
大体积截图/trace、Wasm 构建产物或旧样例文件名变更。

## 发布检查结果

合并后新工作树的 main 代码检查点 `8cea7df`：

| 检查 | 结果 | 本地证据 |
| --- | --- | --- |
| 无需游戏资源的回归矩阵 | 13/13 PASS；6 种 VM 配置输出一致 | `build/check-wip/report.json` |
| 标准版，Wasm 异常 | 55/55 编译并链接；无构建中源码变化 | `build/web-wasm-exceptions-publish/build.json`、相应 `-objects/compile-report.json` |
| 标准版，JS 异常 | 55/55 编译并链接；无构建中源码变化 | `build/web-publish/build.json`、相应 `-objects/compile-report.json` |
| 标准构建隔离 | 两版都未启用 Replay trace/boundary 编译标记 | 两个 `build.json` 的 flags |
| 原生桌面 / 其他作品 / 手机 / 整局 | 本次未完成验收 | 不可由以上 PASS 推断 |

归一化 VM 文本输出 SHA-256：
`39aa29134cf62a0015ff4bf496e3f07f2ab210052c9db4f0ebe98c317cd1e919`。
本机标准 Wasm 二进制 SHA-256（不是跨工具链可复现性保证）：

```text
Wasm exceptions (3747826 bytes): 7405e3714d72960deb1ac82618b7919cd6474b056c22b24b515d82186352791a
JS exceptions   (4171661 bytes): 935fa46c4f55bbb896877d826a0872a81229e5f40ed8048d4515b2934c5364c6
```

以上是本地验收记录，不是 GitHub CI 或公开运行包。后续文档提交没有改动运行代码。
