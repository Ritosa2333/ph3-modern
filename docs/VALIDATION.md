# 验证指南

## 基础回归

```sh
npm run check
npm run build
npm run build:js
npm run test:browser
```

基础矩阵检查 VM 的类型、数值位模式、数组与写时复制、参数、微线程、暂停恢复、事件重入、异常展开、容器生命周期、平台编码和采样完整性。优化、原实现、两种异常模式、旧指令布局与 SIMD 的 VM 输出按哈希对照。AddressSanitizer 检查相应 VM 和平台构建。

报告写入 `build/check/`。CI 在 Windows 与 Linux 执行基础矩阵及两种标准 Web 构建。浏览器测试从生产 GLSL 读取代码，比较混合和 alpha test 的颜色与深度结果。

## 游戏状态与图像

先完成 `npm run build:verify`，按[开发指南](DEVELOPMENT.md)准备本地资源与特效。使用原始 Replay 和已有基准目录：

```sh
npm run profile -- --runtime=build/web-verify-wasm-exceptions --fixture=/path/to/replay.dat --game-root=/path/to/dnh --font=/path/to/font.ttf --stage=1 --fast=8 --trace --trace-stages=2 --seconds=300 --capture-frames=1:600,1:8500,1:10500,1:12500,2:600,2:11000 --out=build/verify-prefix
python tools/compare-campaign-evidence.py /path/to/reference build/verify-prefix --ignore-fps-rect=980,790,1080,815
```

报告核对 Replay 哈希、完整 tick 序列、结束原因和请求截图。1100×825 图像中，FPS 数字区域为 `(980,790)-(1080,815)`，其他像素参与对照。原始整图差异同时保存。

基准采集使用[独立入口](../tools/replay/golden.py)。普通对照读取既有基准。完整 Replay 验收按录像声明的关卡、帧数和分数检查结束点；前缀对照按指定区间记录。

## VM 性能对比

```sh
npm run benchmark:vm -- --baseline=e17060c --rounds=8 --tag=release-review
```

工具从指定 Git 提交提取源码快照，以同一 C++ 输入和编译参数构建两份 Wasm。编译结束后交替运行 AB、BA，每个内核预热后采样六次，报告保存源码及 Wasm 哈希、全部样本与中位数。四个内核分别测量算术、函数调用、嵌套数组及字符串转换。解析发生在计时区间之外。

每次使用新标签，测量期间单独运行该任务。省略 `--baseline` 时，对比当前 VM 的融合与原运算调度。

## 浏览器性能

```sh
npm run profile -- --runtime=build/web-wasm-exceptions --fixture=/path/to/replay.dat --game-root=/path/to/dnh --font=/path/to/font.ttf --stage=1 --throttle=4 --until-frame=10000 --seconds=300 --out=build/performance
python tools/summarize-campaign-runs.py build/performance --out=build/performance-summary.json
```

固定资源、Replay、逻辑区间、窗口、浏览器和 CPU 降速条件，交替运行基线与候选。记录回调 P95/P99、逻辑频率、呈现间隔与丢失的采样窗口。汇总按真实计数加权，并区分 CPU 截止时间与呈现节奏。

CPU profiler、逐 tick trace、加速播放、截图和 GL 内存观察各自用于归因。普通计时采集使用标准构建与独立运行。真机记录设备、系统、浏览器、显示密度和测试区间，长时间游玩另记温度与持续时长。

当前结果见[适配表](../ADAPTATION.md)，按日期的测量见[历史记录](../archive/README.md)。
