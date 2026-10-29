# ph3-modern

在浏览器中运行 Touhou Danmakufu ph3。项目沿用原版解析器、脚本 VM 和游戏系统，以 SDL3、WebGL2 与 WebAudio 接入现代浏览器，并支持 eagler-touhou 的资源与用户文件协议。

通用 VM 优化与平台适配共同降低运行开销：标量内联、参数直传、内置函数快速路径和绘制合批服务于原脚本执行。RMX5080 主菜单的实机 A/B 中，直接顶点上传将呈现频率从约 6 FPS 提升至约 60 FPS；44,676 个已覆盖逻辑 tick 与 11 张游戏截图通过固定基准对照。设备、构建和测量范围见[手机渲染记录](archive/performance-20261007/README.md)。

项目处于开发阶段，当前集成与回归作品为 Hollow Song of Birds 1.12。作品接入、测试覆盖与运行能力见[适配表](ADAPTATION.md)。

相对 `e17060c` 的八轮 Node/Wasm 对照中，算术和函数调用内核的耗时中位数分别下降 59.5% 与 54.1%，嵌套数组下降 38.9%。[完整性能数据](archive/validation-20261029/README.md)列出四个内核的职责、条件和源码身份。

## 开始开发

安装 Node.js 24、Python 3.12 以上和 Emscripten 6.0.6，设置 `EMSDK` 后运行：

```sh
npm ci --ignore-scripts
python -m pip install -r requirements-dev.txt
npm run check
npm run build
```

本地游戏资源、字体与特效工具的配置见[开发指南](docs/DEVELOPMENT.md)。

| 入口 | 内容 |
| --- | --- |
| [开发文档](docs/README.md) | 构建、架构与验证 |
| [参与开发](CONTRIBUTING.md) | 修改与提交流程 |
| [适配表](ADAPTATION.md) | 当前作品和验证范围 |
| [历史记录](archive/README.md) | 源码来源与性能数据 |

## 来源与许可

原版游戏系统来自 [james7132/Danmakufu-ph3](https://github.com/james7132/Danmakufu-ph3)，基线为 `5c92f28e17b6ef3721a3ac4caa9de3af3164b51e`。DAT/zlib 读取机制来自 Woo 修订 `900aa3ebeb329187e8c2852b8ad41caa5200ca9e`，来源登记见[源码导入记录](archive/import-20260930/README.md)。

原代码使用 [NYSL](LICENSE)，随附组件适用各自许可证。游戏素材和字体由使用者按各自许可提供。
