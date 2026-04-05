# Audio_phone_to_pc
**English** | [简体中文](./README.zh_CN.md)

`Audio_phone_to_pc` is a Windows Bluetooth audio receiver utility based on the open-source project `AudioPlaybackConnector` by Richard Yu.

It lets you route audio from a phone or tablet to your PC through Bluetooth audio playback on Windows 10 2004+ / Windows 11, while keeping the workflow lightweight in the notification area.

## What This Version Adds
- Tray-based Bluetooth audio receiver control.
- Chinese and English UI switching.
- Automatic retry when connection fails or drops unexpectedly.
- Cleaner status text focused on daily use instead of codec details.
- Better multi-device switching behavior.
- Fast access to Bluetooth, device, and sound settings.

## Why I Maintain This Fork
I wanted a version that fits my own daily workflow better, especially for:
- switching between phone and PC more smoothly,
- reducing manual reconnect steps after connection failures,
- improving the UI language experience for Chinese and English users,
- making the app easier to understand for non-technical users.

## Upstream Credit
This project is based on:
- Original project: `AudioPlaybackConnector`
- Original author: Richard Yu
- Original repository: https://github.com/ysc3839/AudioPlaybackConnector

This fork keeps the original open-source license and builds on top of the original implementation with additional UX and reliability improvements.

## Maintainer
- GitHub: https://github.com/Braveliu66

## Main Features
- Connect a Bluetooth audio source from your phone to your PC.
- Manage connections from the Windows notification area.
- Reconnect saved devices on next startup.
- Retry automatically after unexpected disconnection or timeout.
- Toggle multi-device behavior.
- Switch UI language between English and Simplified Chinese.

## Usage
1. Build or download `Audio_phone_to_pc`.
2. Pair your phone or tablet in Windows Bluetooth settings.
3. Click the tray icon and choose the device you want to connect.
4. Play audio on your phone and listen on your PC.

## Notes
- Requires Windows 10 version 2004 or later.
- Bluetooth audio playback support depends on the Windows Bluetooth stack and the remote device.

## License
This project remains under the MIT License.

Please keep the original copyright notice from Richard Yu when redistributing substantial portions of the code.
