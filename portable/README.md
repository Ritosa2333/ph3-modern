# 浏览器适配

[BrowserMain.cpp](BrowserMain.cpp)实现浏览器帧循环、输入、加载与观测，`input/` 提供触控处理，`replay/` 提供构建时选择的观察接口。`web/` 是当前 HSoB 页面与 host 协议桥接。

构建和运行使用仓库根目录的[开发指南](../docs/DEVELOPMENT.md)。工具位于 `tools/`，回归位于 `tests/`，模块职责见[架构](../docs/ARCHITECTURE.md)。
