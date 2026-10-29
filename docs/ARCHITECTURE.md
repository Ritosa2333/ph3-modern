# 引擎与 Web 平台

ph3-modern 将原版脚本执行和游戏系统接到 SDL3、WebGL2、WebAudio 与浏览器文件系统。脚本语法、对象生命周期和游戏状态沿原引擎维护，平台层承担设备与操作系统接口。

## 目录

| 目录 | 职责 |
| --- | --- |
| `source/GcLib/gstd/` | 解析器、VM、通用容器和文件接口 |
| `source/GcLib/directx/` | 原有图形与脚本对象接口 |
| `source/GcLib/portable/` | SDL3、GLES3、音频、字体、路径和编码实现 |
| `source/TouhouDanmakufu/` | 游戏系统、关卡、对象、Replay 与原生入口 |
| `portable/` | 浏览器帧循环、触控、Replay 观察接口及 HSoB 网页适配 |
| `tools/` | 构建、资源转换、本地服务与性能测量 |
| `tests/` | 自动回归与独立 C++ 测试输入 |
| `docs/` | 当前架构、开发和验证指南 |
| `archive/` | 按主题和日期保存的来源及测量记录 |

## 脚本与呈现

[脚本 VM](../source/GcLib/gstd/Script.cpp)使用原版解析、字节码和调度。数值、参数传递和内置函数的快速路径按操作数类型、原函数身份与字节码形状选择，其他输入进入对应通用实现。原实现的对照开关服务于差分回归。

[渲染后端](../source/GcLib/portable/DxDevice.cpp)将 D3D9 状态与绘制接口映射到 GLES3。相邻绘制合批、状态缓存、顶点转换及特效 uniform 缓存减少提交开销。顶点按当前绘制的数据量直接上传。纹理缓存按预算管理缓存引用，活跃纹理和渲染目标由使用方持有。

[浏览器入口](../portable/BrowserMain.cpp)协调固定逻辑步进、呈现、输入与加载。SDL 管理窗口像素密度和旋转后的尺寸，游戏背缓冲使用作品定义的分辨率。

## eagler-touhou 接入

[HSoB 页面适配](../portable/web/shell.mjs)准备作品资源、用户目录和 Runtime 启动参数；[host 桥接](../portable/web/eagler-host.mjs)对接 `eagler-touhou/1` 协议。

启动器管理产品选择、安装内容和 Runtime 生命周期。引擎消费传入的资源与用户目录，用户目录保存存档和 Replay。Runtime 二进制、游戏内容和用户文件分别维护身份。

跨作品的平台能力优先接入 eagler-touhou 的共用接口，作品资源路径和游戏菜单由适配层处理。当前 HSoB 的 `script/th_hsob` 资源发现、关卡预热规则及特效集合构成作品接入配置；扩展作品时从这些边界入手。

## 构建与观察

标准构建输出游戏 Runtime；`boundary` 增加关卡边界观察；`verify` 增加逐 tick 状态和 GL 错误检查。性能测量使用标准构建，状态与截图对照使用验证构建。构建清单绑定各自源码与产物。
