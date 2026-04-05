# Audio_phone_to_pc
[English](./README.md) | **简体中文**

`Audio_phone_to_pc` 是一个基于 `AudioPlaybackConnector` 二次开发的 Windows 蓝牙音频接收工具。

它可以让手机或平板通过蓝牙把音频传到电脑播放，并且把常用操作保留在系统托盘里，适合日常快速连接使用。

## 这个版本做了什么
- 保留托盘区快速连接蓝牙音频的体验。
- 新增中英文界面切换。
- 新增连接失败或异常断开后的自动重试。
- 去掉偏技术化的编码显示，状态文案更贴近日常使用。
- 优化多设备切换时的交互体验,提高连接稳定性以及修复了一些bug。
- 保留蓝牙、设备、声音设置的快捷入口。

## 主要功能
- 将手机蓝牙音频连接到电脑播放。
- 从 Windows 托盘区管理连接。
- 支持下次启动时恢复上次连接设备。
- 支持连接失败或意外断开后的自动重试。
- 支持多设备连接策略切换。
- 支持英文 / 简体中文界面切换。

## 使用方法
1. 下载 `Audio_phone_to_pc`
2. 链接: https://github.com/Braveliu66/Audio_phone_to_pc/releases/download/exe/Audio_phone_to_pc.zip
3. 先在 Windows 蓝牙设置中配对你的手机或平板。
4. 点击托盘图标，选择想连接的设备。
5. 在手机上播放音频，即可从电脑端收听。

## 说明
- 需要 Windows 10 2004 及以上版本，或 Windows 11。
- 蓝牙音频接收能力依赖 Windows 蓝牙栈以及目标设备本身的支持情况。


## 原项目说明与致谢
本项目基于以下开源项目继续开发：
- 原项目：`AudioPlaybackConnector`
- 原作者：Richard Yu
- 原仓库：https://github.com/ysc3839/AudioPlaybackConnector

这个版本在保留原项目开源协议的前提下，加入了我自己的功能改进、交互优化和稳定性修复。
## 许可证
本项目继续使用 MIT License。

如果你分发本项目或其主要代码，请保留原作者 Richard Yu 的版权声明与许可证文本。
