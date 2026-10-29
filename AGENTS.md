# 开发入口

先读 [README.md](README.md)，再按[开发文档](docs/README.md)选择本次任务的主指南。修改源码前阅读[架构](docs/ARCHITECTURE.md)；实现、调试和验收使用[验证指南](docs/VALIDATION.md)。

- 检查分支、远端、工作区状态和相关差异。当前源码在本仓库维护，历史记录按其日期和构建查询。
- 原解析器、VM 与游戏系统定义脚本行为。性能修改使用相同输入对照类型、状态、RNG、结束点和图像。
- 跨作品平台能力对齐 eagler-touhou 的共用接口。HSoB 资源、预热和菜单配置在作品适配边界维护。
- 工具链与浏览器配置使用 `tools/lib/project.mjs`，本地资源配置使用 `tools/project.py`。更新命令时一并更新调用方和指南。
- 自动回归使用 `npm run check`。源码改动运行对应构建，渲染改动运行浏览器检查和固定基准对照。
- 当前指南放在 `docs/`，来源和日期记录放在 `archive/`。新建或移动文档运行 `npm run docs:index` 和 `npm run docs:check`。

协作与提交入口见 [CONTRIBUTING.md](CONTRIBUTING.md)。
