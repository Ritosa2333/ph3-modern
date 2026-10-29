# 开发指南

## 环境

使用 Node.js 24、Python 3.12 以上及 Emscripten 6.0.6。`EMSDK` 指向已安装的 SDK，`PH3_PYTHON` 可选择 Python 可执行文件。

首次取源码可按构建需要选择目录：

```sh
git clone --filter=blob:none --sparse https://github.com/Ritosa2333/ph3-modern.git
cd ph3-modern
git sparse-checkout set source portable tools tests docs archive .github DirectX9c
```

在仓库根目录执行：

```sh
npm ci --ignore-scripts
python -m pip install -r requirements-dev.txt
npm run check
npm run build
```

构建从 `DnhExecutor.vcxproj` 读取完整执行器源码，用 SDL3 浏览器入口替换 Windows 入口，再编译平台实现。默认使用 O3、LTO 和 Wasm 异常处理。

| 命令 | 产物或用途 |
| --- | --- |
| `npm run build` | 标准 Runtime，输出到 `build/web-wasm-exceptions/` |
| `npm run build:verify` | 逐 tick 观察与 GL 错误检查，输出到 `build/web-verify-wasm-exceptions/` |
| `npm run build:js` | JavaScript 异常处理模式，输出到 `build/web/` |
| `npm test` | VM、平台、容器、采样及证据工具回归 |
| `npm run test:browser` | 浏览器 WebGL2 着色器逐字节对照 |
| `npm run docs:index` | 生成文档总表 |
| `npm run docs:check` | 检查总表、内部链接和标题锚点 |

`build.json` 登记源码哈希、工具链、参数与输出文件身份。带 `--label=名称` 的构建写入独立目录，便于对比。`PH3_JOBS` 设置并行编译数，范围为 1 到 8。`PH3_EXTRA_FLAGS` 接收额外编译参数的 JSON 数组。

## 本地运行 HSoB

准备 Hollow Song of Birds 1.12 的本地安装和可用日文字体。游戏目录应包含 `script/th_hsob/Main.dnh`。

```sh
npm run effects -- --game-root=/path/to/dnh --glslang=/path/to/glslang --spirv-cross=/path/to/spirv-cross
npm run dev -- --game-root=/path/to/dnh --font=/path/to/japanese-font.ttf
```

打开 `http://127.0.0.1:8797/`，点击页面开始。预览服务读取原安装，派生特效写入 `build/effects/`。特效转换接受单 pixel pass 的 D3D9 FX，sidecar 保存原 FX 字节供运行时核对。

可通过 `PH3_GAME_ROOT`、`PH3_FONT` 和 `PH3_SHADER_TOOLS` 配置本地资源。转换器也接受 `--shader-tools`；该目录使用 `glslang/bin/glslang.exe` 与 `cross-build/Release/spirv-cross.exe` 的布局。独立安装的工具使用上方两个显式参数。

## 浏览器工具

`npm ci` 安装仓库自己的 Puppeteer 依赖。`PH3_CHROME` 指定 Chrome 或 Chromium，工具也会查找常见安装位置。既有环境变量 `HSOB_CHROME`、`HSOB_LAUNCHER` 可继续用于本地接入。

[Python 浏览器工具](../tools/browser-smoke.py)与 [Replay 菜单驱动](../tools/replay/run-browser.py)使用 Playwright。运行前执行：

```sh
python -m pip install -r requirements-browser.txt
```

浏览器回归和性能命令见[验证指南](VALIDATION.md)。

## 独立源码参考

`tools/replay/` 提供原版及 Woo 源码的 Replay 对照工具。原版基线可从本仓库完整历史读取；浅克隆先执行 `git fetch --unshallow`。Woo 参考通过 `PH3_WOO_SOURCE` 指向包含固定修订的独立源码仓库，`PH3_ORIGINAL_SOURCE` 可指定另一份原版仓库。工具在生成输入前核对提交身份，输出写入 `build/`。

原生参考构建使用 Windows MSVC Build Tools。对应入口为 `python tools/replay/build-native.py --family=original`，观察接口由 `python tools/replay/native-hooks.py --family=original` 生成。默认 Web 构建和基础回归使用当前仓库源码。
