# 参与开发

先按[开发指南](docs/DEVELOPMENT.md)运行当前版本，再依据[架构](docs/ARCHITECTURE.md)找到行为所属模块。VM、渲染或调度修改同时阅读[验证指南](docs/VALIDATION.md)，以原行为和固定输入做对照。

通用平台代码集中在 `source/GcLib/portable/`，工具配置集中在 `tools/lib/project.mjs`。新增作品从适配入口及资源配置接入。修改接口、命令或目录时，同步更新调用方、相关测试和当前指南，并裁掉被替代的实现。

提交前运行对应行为检查、构建与 `npm run docs:check`。新建或移动文档后先运行 `npm run docs:index`。测试结论写入[适配表](ADAPTATION.md)，来源和按日期的测量放入 `archive/<主题-日期>/`，相关 JSON 与说明同目录保存。

改动按用途分别提交，标题使用简短中文。GitHub PR 标题、正文和评论交由维护者审阅后发布。
