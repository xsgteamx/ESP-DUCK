# ESP-DUCK
ESP32s3 Send smth as a USB HID Virtual Keyborad with Web Console

> 当前项目是一个基于 ESP32 / ESP32-S3 的 USB HID 控制项目，用于通过浏览器 Web 页面控制开发板向目标电脑发送键盘输入、命令文本或脚本内容。
> 本项目主要用于个人开发、内网调试、自动化输入和受限环境下的文本传送测试。

## 功能特性

- 支持通过 Web 页面连接 ESP 开发板
- 支持 USB HID 键盘模拟输入
- 支持发送普通文本、命令和脚本片段
- 支持中文内容预处理，例如拼音转写后再发送
- 支持配置项抽象，避免把密码、Token、Wi-Fi 信息写死在代码中
- 后续计划支持上电恢复默认设置和物理按键快捷操作

## 项目结构

    .
    ├── README.md
    ├── .gitignore
    ├── config.example.h
    ├── src/
    │   └── main.cpp
    ├── data/
    │   └── index.html
    └── docs/
        └── notes.md

实际目录可能根据 Arduino IDE、PlatformIO 或其他构建方式有所不同。

## 配置说明

为了避免敏感信息泄露，仓库中不提交真实配置文件。

说明：

| 配置项 | 说明 |
| --- | --- |
| `WIFI_SSID` | Wi-Fi 名称 |
| `WIFI_PASSWORD` | Wi-Fi 密码 |
| `WEB_AUTH_TOKEN` | Web 控制端访问 Token |
| `DEVICE_NAME` | 设备名称 |
| `DEFAULT_DELAY_MS` | 默认输入间隔 |

## 使用方式

### 1. 准备开发环境

根据你的项目类型选择一种方式：

- Arduino IDE
- PlatformIO
- ESP-IDF

如果使用 Arduino IDE，请确认已经安装对应 ESP32 开发板支持包。

### 2. 创建本地配置文件

复制示例配置：

    cp config.example.h config.h

修改 `config.h`，填入本地 Wi-Fi、Token 和设备配置。

### 3. 编译并上传固件

使用 Arduino IDE 或 PlatformIO 编译并上传到 ESP32 / ESP32-S3 开发板。

PlatformIO 示例命令：

    pio run
    pio run --target upload

### 4. 上传 Web 文件

如果项目使用 LittleFS / SPIFFS 存放 Web 页面，需要上传 `data/` 目录。

PlatformIO 示例命令：

    pio run --target uploadfs

Arduino IDE 可使用 LittleFS 上传插件。

### 5. 连接设备

开发板启动后，通过串口日志查看设备 IP 或访问地址。

打开浏览器访问设备 Web 页面，连接后即可发送文本或命令。


## 常见问题

### 连接 Web 页面后浏览器卡顿

如果浏览器在刚连接开发板后就明显卡顿，优先检查以下内容：

1. Web 页面是否存在无限读取串口数据的循环
2. 是否重复注册了 `reader`、`listener`、`setInterval`
3. 是否把大量日志持续追加到 DOM
4. 设备端是否在连接后持续高速输出日志
5. 页面是否没有正确释放串口 reader
6. 是否缺少限流、分页或日志上限

建议先使用最简单的连接页面测试，只保留连接、断开和少量日志输出，用于确认问题是否来自主页面逻辑。


### 中文内容发送异常

USB HID 键盘输入对中文支持有限。建议将中文内容提前转换为拼音、英文或 Base64 后再发送。

本项目后续计划提供专门的内容整理工具，用于生成：

- 原文版
- 拼音转写版

方便在受限环境中通过开发板传送内容。

## 开发计划

- [ ] 优化 Web 串口连接稳定性
- [ ] 增加日志输出限流
- [ ] 增加断开连接后的资源释放
- [ ] 支持上电恢复默认配置
- [ ] 支持物理按键快捷操作
- [ ] 支持文本发送队列
- [ ] 支持中文内容拼音转写
- [ ] 支持配置导入和导出

## 免责声明

本项目仅用于个人学习、自动化输入测试、内网调试和合法授权环境。

请勿将本项目用于未授权设备、未授权系统或任何违反法律法规的用途。

使用本项目产生的风险由使用者自行承担。
```
