# Global Input Bridge 全局输入桥

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE) [![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.7-313131?logo=unrealengine)](https://unrealengine.com) [![Platform](https://img.shields.io/badge/Platform-Windows%20x64-0078D6?logo=windows11)](https://learn.microsoft.com/windows/)
[![Version](https://img.shields.io/badge/Version-1.0.4-brightgreen)](CHANGELOG.md) [![Stars](https://img.shields.io/github/stars/xiaoyaomoyor/GlobalInputBridge)](https://github.com/xiaoyaomoyor/GlobalInputBridge/stargazers) [![Forks](https://img.shields.io/github/forks/xiaoyaomoyor/GlobalInputBridge)](https://github.com/xiaoyaomoyor/GlobalInputBridge/network/members) [![Issues](https://img.shields.io/github/issues/xiaoyaomoyor/GlobalInputBridge)](https://github.com/xiaoyaomoyor/GlobalInputBridge/issues)

> **Vibe-Coding 参与声明**：本插件的开发过程有 AI 辅助编程（Vibe Coding）参与。

[English](README.md) | **简体中文**

Global Input Bridge 是一个功能丰富的 Unreal Engine Windows 输入插件，向游戏线程提供**不依赖窗口焦点的全局键盘与鼠标输入**——即使你的 UE 应用位于后台也能持续接收。

它面向"UE 作为伴随程序运行在其他前台软件之后"的工作流：**虚拟主播直播**（玩 FPS 时同步虚拟角色的视角与移动）、宏面板、输入分析、覆盖层工具等场景。

> 亮点：即使前台是会把系统光标钉死在屏幕中心的 Raw Input 类 FPS 游戏（如猎杀对决），鼠标视角增量依然持续可用——因为移动数据读取自 `WM_INPUT` 原始相对增量，与游戏自身消费的数据同源。

## 功能特性

- **全局键盘** —— 后台 Windows Raw Input 工作线程（`RIDEV_INPUTSINK`）；无论窗口是否聚焦都能接收按键，支持多设备聚合与热插拔。
- **全局鼠标移动** —— 四种跟踪模式，默认使用 Raw Input 相对增量：
  - `Raw Input`（默认）：按帧聚合的 `RAWMOUSE.lLastX/lLastY` 设备计数——不受光标锁定、指针加速、DPI 缩放影响。
  - `Polling`：通过 `GetCursorPos` 获取桌面像素增量。
  - `Buttons Only` / `Disabled`。
- **Global Input Action Event** 蓝图节点 —— 类似 Enhanced Input 的生命周期（`Started` / `Triggered` / `Completed`），支持组合键与可选的互斥修饰键匹配，无需手动 Bind/Unbind。
- **状态查询** —— `Is Global Key Down`、`Was Global Key Pressed/Released This Frame`、`Get Pressed Global Keys`、修饰键状态。
- **广播事件** —— `On Global Key Event` 与 `On Global Mouse Move`。
- **事件过滤** —— 允许列表或排除列表，可拦截 `On Global Key Event` 广播与 `Global Input Action Event` 的 Started/Triggered，不影响状态采集。
- **调试支持** —— `Get Global Input Debug Info` 快照与可配置日志等级。
- **自动化测试** —— 覆盖状态管理器、组合键绑定管理器、子系统生命周期、按键映射与编辑器节点编译。

插件是纯粹的输入**读取者**：不注入、不合成输入，也不替代项目中的 Enhanced Input。

## 环境要求

- Unreal Engine 5.7（开发与测试环境；其他 UE 5.x 版本可能可以编译但未测试）
- Windows x64（插件仅在 Win64 平台加载）

## 安装

1. 将本仓库克隆或下载到你的项目中：

   ```
   <YourProject>/Plugins/GlobalInputBridge
   ```

2. 重新生成工程文件并编译（或直接启动工程让编辑器编译插件）。
3. 如有提示则启用插件。配置入口：`Project Settings > Plugins > Global Input Bridge`。

## 快速上手（蓝图）

> 插件内容含有 DemoMap，可打开关卡查看蓝图示例。

1. 用 `Get Engine Subsystem` 获取 `GlobalInputSubsystem`。

   ![Start Listening](Images/GlobalInputBridge_Shot01.png)

2. 在设置中开启 **Auto Start**，或自行调用 `Start Listening`（Commandlet 与专用服务器永不自动启动）。
3. 持续行为查询 `Is Global Key Down`。
4. 一次性行为查询 `Was Global Key Pressed This Frame` / `Was Global Key Released This Frame`。

   ![Key State Queries](Images/GlobalInputBridge_Shot04.png)

5. 鼠标视角每 Tick 读取 `Get Global Mouse Delta`，或绑定 `On Global Mouse Move`。

   ![Mouse Delta](Images/GlobalInputBridge_Shot03.png)

6. 按键动作直接添加紫色 **Global Input Action Event** 节点，在 Details 中配置 Key、Modifiers 与可选的 `Exact Modifiers (Exclusive)`。

   ![Global Input Action Event](Images/GlobalInputBridge_Shot02.png)

典型的视角同步方案：每 Tick 累加 `Get Global Mouse Delta`，乘以灵敏度系数，驱动 Aim Offset。Raw Input 增量是设备计数（无指针加速），量级与鼠标上报的计数一致，按需调整缩放系数即可。

## 鼠标跟踪模式

在 `Project Settings > Plugins > Global Input Bridge` 中配置：

| 模式               | 移动增量                   | 按钮   | 桌面位置 | 说明                                 |
| ------------------ | -------------------------- | ------ | -------- | ------------------------------------ |
| `Raw Input`（默认）| 原始相对计数（`WM_INPUT`） | 轮询   | 轮询     | 锁鼠 FPS 中依然可用；与游戏消费的数据同源 |
| `Polling`          | 桌面像素差                 | 轮询   | 轮询     | 桌面语义；锁鼠游戏中增量为零           |
| `Buttons Only`     | 无                         | 轮询   | 不查询   | 永不广播 `On Global Mouse Move`       |
| `Disabled`         | 无                         | 不轮询 | 不查询   | 所有模式下键盘监听均不受影响           |

## 工作原理

- 专用工作线程运行一个隐藏的 Win32 消息窗口，以 `RIDEV_INPUTSINK` 注册键盘与（Raw Input 模式下的）鼠标设备集合，无需焦点即可全系统接收输入。数据包经单生产者/单消费者队列传递，在引擎子系统 Tick 中于游戏线程消费。
- 鼠标按钮与桌面光标位置在游戏线程通过 `GetAsyncKeyState` / `GetCursorPos` 轮询。
- **注册心跳**：Windows 规定同一进程内每个设备类只能有一个 Raw Input 接收窗口——后注册者生效。UE 自身的视口高精度鼠标模式（视口点击、PIE、鼠标捕获）会反复抢占该注册位。启用鼠标 Raw Input 时，工作线程以 500ms 心跳在停止收到鼠标输入后自动重新注册夺回，最多丢失半秒移动数据。跨进程注册互不冲突，前台游戏不受任何影响。
- 所有 UObject 访问、状态与蓝图广播都在游戏线程；工作线程只接触 Win32 API 与队列。

## 事件过滤

`Set Global Input Event Filter` 对 `On Global Key Event` 与 `Global Input Action Event` 启用过滤：

- `Exclude Mode = false`（默认）：Keys 为允许列表；空数组停止所有按键事件广播。
- `Exclude Mode = true`：Keys 为排除列表；空数组不排除任何按键。

被过滤的按键不会广播事件，也无法触发新的动作（Started）；过滤前已激活动作不再触发 `Triggered`，但其 `Completed` 仍会正常发出，避免蓝图侧 Started 悬空。状态查询默认始终反映物理按键（帧边沿与修饰键状态亦然）；轮询类逻辑（如移动镜像）若需要与过滤保持一致，勾选 `Is Global Key Down` / `Was Global Key Pressed This Frame` / `Get Pressed Global Keys` 上的 **Respect Event Filter** 复选框即可，被过滤的键将按未按下处理。`Was Global Key Released This Frame` 与修饰键状态刻意不受过滤——收尾信号永远放行，与 Completed 语义对齐。`Is Global Key Event Suppressed` 保留为通用构建块。`Clear Global Input Event Filter` 恢复完整广播。

![Event Filtering](Images/GlobalInputBridge_Shot05.png)

## 调试

`Get Global Input Debug Info` 返回快照：监听状态、键盘设备数、按下按键、鼠标位置/增量有效性与过滤状态。

将 **Log Level** 设为 `Verbose` 并在 Output Log 中筛选 `LogGlobalInput`，可查看 Worker 生命周期、原始数据包与注册夺回日志（`Re-armed mouse Raw Input`）。

## 限制与说明

- 仅支持 Windows（所有平台代码由 `PLATFORM_WINDOWS` 保护；其他平台不加载该模块）。
- 纯被动读取——插件不能阻断或修改其他程序的输入，也不能作为输入注入器使用。
- Raw Input 鼠标增量是设备计数；如需像素语义请自行换算。
- 当你自己的 UE 视口捕获鼠标时（PIE、视口点击），插件可能在 500ms 内夺回 Raw Input 注册；编辑器/PIE 鼠标随后退回传统消息，仍然可用。

## 许可证

基于 [MIT License](LICENSE) 发布 — Copyright (c) 2026 xiaoyaomoyor。
